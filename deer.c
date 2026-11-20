#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX 6
#define ARRIVED SIGRTMIN
#define READY (SIGRTMIN + 1)

const char *territories[MAX] = {"Карпати", "Чорногора", "Свидовець",
                                "Горгани", "Мармароси", "Боржава"};
const char *persons[MAX] = {"Олена Шевчук", "Іван Коваленко", "Петро Бондар",
                            "Марія Ткаченко", "Тарас Мельник", "Оксана Кравець"};

void expedition(int id, int fd)
{
    char name[32] = "";
    srand(getpid());
    sleep(1 + rand() % 3);
    printf("[%d] Експедиція %d (%s) прибула на місце\n", getpid(), id + 1, persons[id]);
    sigqueue(getppid(), ARRIVED, (union sigval){.sival_int = id});
    read(fd, name, sizeof(name) - 1);
    close(fd);
    printf("[%d] Отримано ідентифікаційний текст: %s\n", getpid(), name);
    sigqueue(getppid(), READY, (union sigval){.sival_int = id});
    exit(0);
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
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, ARRIVED);
    sigaddset(&set, READY);
    sigprocmask(SIG_BLOCK, &set, NULL);
    int fd[MAX][2];
    pid_t pids[MAX];
    printf("Відправлено експедицій: %d, тривалість спостереження: %d с\n", n, duration);
    for (int i = 0; i < n; i++) {
        pipe(fd[i]);
        pids[i] = fork();
        if (pids[i] == 0) {
            close(fd[i][1]);
            expedition(i, fd[i][0]);
        }
        close(fd[i][0]);
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
    for (int i = 0; i < n; i++)
        waitpid(pids[i], NULL, 0);
    printf("Усі експедиції завершили роботу\n");
    return 0;
}
