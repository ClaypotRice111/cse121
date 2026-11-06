
#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <gpiod.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define GPIO_CHIP "/dev/gpiochip0"
#define LED_GPIO 17
#define UNIT_MS 60

static volatile sig_atomic_t running = 1;

static const char *morse[] = {
    ".-", "-...", "-.-.", "-..", ".", "..-.",
    "--.", "....", "..", ".---", "-.-", ".-..",
    "--", "-.", "---", ".--.", "--.-", ".-.",
    "...", "-", "..-", "...-", ".--", "-..-",
    "-.--", "--..",
    "-----", ".----", "..---", "...--", "....-",
    ".....", "-....", "--...", "---..", "----."
};

static void signal_handler(int sig)
{
    (void)sig;
    running = 0;
}

static void sleep_ms(unsigned int ms)
{
    struct timespec req = {
        .tv_sec = ms / 1000,
        .tv_nsec = (long)(ms % 1000) * 1000000L
    };

    while (running && nanosleep(&req, &req) == -1) {
        if (errno != EINTR) {
            perror("nanosleep");
            running = 0;
            break;
        }
    }
}

static const char *get_morse(char c)
{
    unsigned char ch = (unsigned char)c;
    ch = (unsigned char)toupper(ch);

    if (ch >= 'A' && ch <= 'Z') {
        return morse[ch - 'A'];
    }

    if (ch >= '0' && ch <= '9') {
        return morse[26 + ch - '0'];
    }

    return NULL;
}

static int set_led(struct gpiod_line *line, int value)
{
    if (gpiod_line_set_value(line, value) < 0) {
        perror("gpiod_line_set_value");
        running = 0;
        return -1;
    }
    return 0;
}

static void send_character(struct gpiod_line *line,
                           const char *code)
{
    for (size_t i = 0; code[i] && running; i++) {
        if (set_led(line, 1) < 0)
            return;

        if (code[i] == '.') {
            sleep_ms(UNIT_MS);
        } else {
            sleep_ms(3 * UNIT_MS);
        }

        if (set_led(line, 0) < 0)
            return;

        if (code[i + 1] != '\0' && running) {
            sleep_ms(UNIT_MS);
        }
    }
}

static void send_message(struct gpiod_line *line,
                         const char *message)
{
    int previous = 0;
    // 0 = none, 1 = character, 2 = word gap

    for (size_t i = 0; message[i] && running; i++) {
        if (message[i] == ' ') {
            if (previous == 1) {
                previous = 2;
            }
            continue;
        }

        const char *code = get_morse(message[i]);

        if (code == NULL) {
            continue;
        }

        if (previous == 1) {
            sleep_ms(3 * UNIT_MS);
            printf(" ");
        } else if (previous == 2) {
            sleep_ms(7 * UNIT_MS);
            printf(" / ");
        }

        if (!running)
            break;

        printf("%s", code);
        fflush(stdout);

        send_character(line, code);
        previous = 1;
    }

    printf("\n");
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr,
                "Usage: %s <times> \"message\"\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    char *end = NULL;
    errno = 0;
    long repeat = strtol(argv[1], &end, 10);

    if (errno != 0 || *end != '\0' ||
        repeat <= 0 || repeat > 1000000) {
        fprintf(stderr, "Invalid repeat count\n");
        return EXIT_FAILURE;
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    struct gpiod_chip *chip =
        gpiod_chip_open(GPIO_CHIP);

    if (chip == NULL) {
        perror("gpiod_chip_open");
        return EXIT_FAILURE;
    }

    struct gpiod_line *led =
        gpiod_chip_get_line(chip, LED_GPIO);

    if (led == NULL) {
        perror("gpiod_chip_get_line");
        gpiod_chip_close(chip);
        return EXIT_FAILURE;
    }

    if (gpiod_line_request_output(led, "morse_led", 0) < 0) {
        perror("gpiod_line_request_output");
        gpiod_chip_close(chip);
        return EXIT_FAILURE;
    }

    for (long i = 0; i < repeat && running; i++) {
        if (i > 0) {
            sleep_ms(7 * UNIT_MS);
        }

        if (!running)
            break;

        send_message(led, argv[2]);
    }

    set_led(led, 0);

    gpiod_line_release(led);
    gpiod_chip_close(chip);

    return EXIT_SUCCESS;
}
