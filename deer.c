#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/msg.h>

#define MAX 6
#define ARRIVED SIGRTMIN
#define READY (SIGRTMIN + 1)

struct observation {
    long mtype;
    char observing_personname[64];
    char territory_name[32];
    int deer_number;
    int deer_age;
    time_t observation_time;
};

#define MSG_SIZE (sizeof(struct observation) - sizeof(long))

const char *territories[MAX] = {"Карпати", "Чорногора", "Свидовець",
                                "Горгани", "Мармароси", "Боржава"};
const char *persons[MAX] = {"Олена Шевчук", "Іван Коваленко", "Петро Бондар",
                            "Марія Ткаченко", "Тарас Мельник", "Оксана Кравець"};
const char *ages[3] = {"молодий", "дорослий", "старий"};
char note[128];

void recall(int sig)
{
    (void)sig;
    write(STDOUT_FILENO, note, strlen(note));
    _exit(0);
}

void expedition(int id, int fd, int qid, int duration)
{
    char name[32] = "";
    srand(getpid());
    sleep(1 + rand() % 3);
    printf("[%d] Експедиція %d (%s) прибула на місце\n", getpid(), id + 1, persons[id]);
    sigqueue(getppid(), ARRIVED, (union sigval){.sival_int = id});
    read(fd, name, sizeof(name) - 1);
    close(fd);
    printf("[%d] Отримано ідентифікаційний текст: %s\n", getpid(), name);
    snprintf(note, sizeof(note), "[%d] Експедиція %s повертається на базу\n", getpid(), name);
    signal(SIGTERM, recall);
    sigqueue(getppid(), READY, (union sigval){.sival_int = id});
    time_t end = time(NULL) + duration;
    for (int k = 0; ; k++) {
        sleep(2 + rand() % 4);
        if (time(NULL) >= end)
            break;
        struct observation o = {1, "", "", 1 + rand() % 12, k % 3, time(NULL)};
        strcpy(o.observing_personname, persons[id]);
        strcpy(o.territory_name, name);
        msgsnd(qid, &o, MSG_SIZE, 0);
    }
    exit(0);
}

void save(FILE *f, struct observation *o)
{
    char t[32];
    strftime(t, sizeof(t), "%d.%m.%Y %H:%M:%S", localtime(&o->observation_time));
    fprintf(f, "%s | %s | %d | %s | %s\n", o->observing_personname, o->territory_name,
            o->deer_number, ages[o->deer_age], t);
    fflush(f);
    printf("Записано: %s, %s, оленів: %d, вік: %s\n", o->territory_name,
           o->observing_personname, o->deer_number, ages[o->deer_age]);
}

int wait_signal(int sig)
{
    sigset_t set;
    siginfo_t info;
    sigemptyset(&set);
    sigaddset(&set, sig);
    sigwaitinfo(&set, &info);
    return info.si_value.sival_int;
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        printf("Використання: %s <кількість експедицій> <тривалість в секундах>\n", argv[0]);
        return 1;
    }
    int n = atoi(argv[1]), duration = atoi(argv[2]);
    if (n < 1 || n > MAX || duration < 1) {
        printf("Кількість експедицій від 1 до %d, тривалість більше 0\n", MAX);
        return 1;
    }
    setbuf(stdout, NULL);
    srand(time(NULL));
    FILE *f = fopen("observations.txt", "a");
    if (f == NULL) {
        perror("observations.txt");
        return 1;
    }
    fseek(f, 0, SEEK_END);
    if (ftell(f) == 0)
        fprintf(f, "observing_personname | territory_name | deer_number | deer_age | observation_time\n");
    fflush(f);
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, ARRIVED);
    sigaddset(&set, READY);
    sigprocmask(SIG_BLOCK, &set, NULL);
    int qid = msgget(IPC_PRIVATE, IPC_CREAT | 0600);
    if (qid == -1) {
        perror("msgget");
        return 1;
    }
    int fd[MAX][2], alive[MAX], left = n;
    pid_t pids[MAX];
    printf("Відправлено експедицій: %d, тривалість спостереження: %d с\n", n, duration);
    for (int i = 0; i < n; i++) {
        pipe(fd[i]);
        pids[i] = fork();
        if (pids[i] == 0) {
            close(fd[i][1]);
            expedition(i, fd[i][0], qid, duration);
        }
        close(fd[i][0]);
        alive[i] = 1;
    }
    for (int i = 0; i < n; i++) {
        int id = wait_signal(ARRIVED);
        printf("Сигнал прибуття від експедиції %d (%d/%d)\n", id + 1, i + 1, n);
    }
    for (int i = 0; i < n; i++) {
        write(fd[i][1], territories[i], strlen(territories[i]));
        close(fd[i][1]);
    }
    for (int i = 0; i < n; i++)
        printf("Експедиція %s готова до роботи\n", territories[wait_signal(READY)]);
    time_t end = time(NULL) + duration, last = 0;
    struct observation o;
    while (time(NULL) < end) {
        if (msgrcv(qid, &o, MSG_SIZE, 0, IPC_NOWAIT) > 0)
            save(f, &o);
        else
            usleep(100000);
        if (time(NULL) != last) {
            last = time(NULL);
            int i = rand() % n;
            if (left > 1 && alive[i] && rand() % 10 == 0) {
                printf("Експедицію %s відкликано через втому\n", territories[i]);
                kill(pids[i], SIGTERM);
                waitpid(pids[i], NULL, 0);
                alive[i] = 0;
                left--;
            }
        }
    }
    for (int i = 0; i < n; i++)
        if (alive[i])
            waitpid(pids[i], NULL, 0);
    while (msgrcv(qid, &o, MSG_SIZE, 0, IPC_NOWAIT) > 0)
        save(f, &o);
    fclose(f);
    msgctl(qid, IPC_RMID, NULL);
    printf("Спостереження завершено, до кінця працювало: %d, відкликано: %d\n", left, n - left);
    return 0;
}
