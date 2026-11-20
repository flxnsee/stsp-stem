#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX 6

const char *persons[MAX] = {"Олена Шевчук", "Іван Коваленко", "Петро Бондар",
                            "Марія Ткаченко", "Тарас Мельник", "Оксана Кравець"};

void expedition(int id)
{
    srand(getpid());
    sleep(1 + rand() % 3);
    printf("[%d] Експедиція %d (%s) прибула на місце\n", getpid(), id + 1, persons[id]);
    exit(0);
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
    pid_t pids[MAX];
    printf("Відправлено експедицій: %d, тривалість спостереження: %d с\n", n, duration);
    for (int i = 0; i < n; i++) {
        pids[i] = fork();
        if (pids[i] == 0)
            expedition(i);
    }
    for (int i = 0; i < n; i++)
        waitpid(pids[i], NULL, 0);
    printf("Усі експедиції завершили роботу\n");
    return 0;
}
