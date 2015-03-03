/*
 * This is free and unencumbered public domain software.
 * For more information see http://unlicense.org/.
 *
 * @author Hans-Harro Horn <h.h.horn@gmail.com> (hhhorn.de)
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <X11/Xlib.h>

static Display *dpy;
long cpu0_work = 0;
long cpu0_total = 0;
long cpu1_work = 0;
long cpu1_total = 0;

void setstatus(char *str) {
    XStoreName(dpy, DefaultRootWindow(dpy), str);
    XSync(dpy, False);
}

int readInt(char *input) {
    FILE *fd;
    int val;

    fd = fopen(input, "r");
    if (fd==NULL)
        return -1;
    fscanf(fd, "%d", &val);
    fclose(fd);
    return val;
}

int getcpu(char *status, size_t size) {
    FILE *fd;
    long jif1, jif2, jif3, jif4, jif5, jif6, jif7;
    long work0, total0, work1, total1;
    int load0, load1, freq;
    char *color0, *color1;

    // ---- LOAD
    fd = fopen("/proc/stat", "r");
    char c;
    while (c != '\n') c = fgetc(fd);
    fscanf(fd, "cpu0 %ld %ld %ld %ld %ld %ld %ld", &jif1, &jif2, &jif3, &jif4, &jif5, &jif6, &jif7);
    work0 = jif1 + jif2 + jif3 + jif6 + jif7;
    total0 = work0 + jif4 + jif5;

    c = 0;
    while (c != '\n') c = fgetc(fd);
    fscanf(fd, "cpu1 %ld %ld %ld %ld %ld %ld %ld", &jif1, &jif2, &jif3, &jif4, &jif5, &jif6, &jif7);
    work1 = jif1 + jif2 + jif3 + jif6 + jif7;
    total1 = work1 + jif4 + jif5;

    fclose(fd);

    load0 = 100 * (work0 - cpu0_work) / (total0 - cpu0_total);
    load1 = 100 * (work1 - cpu1_work) / (total1 - cpu1_total);

    cpu0_work = work0;
    cpu0_total = total0;
    cpu1_work = work1;
    cpu1_total = total1;

    // ---- FREQ
    freq = readInt("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
    if(freq <= 800000)
        color0 = "\x02";
    else if(freq >= 2000000)
        color0 = "\x07";
    else
        color0 = "\x05";

    freq = readInt("/sys/devices/system/cpu/cpu1/cpufreq/scaling_cur_freq");
    if(freq <= 800000)
        color1 = "\x02";
    else if(freq >= 2000000)
        color1 = "\x07";
    else
        color1 = "\x05";

    return snprintf(status, size, "\x08""C%s%3d%%%s%3d%%", color0, load0, color1, load1);
}

int getmem(char *status, size_t size) {
    FILE *fd;
    long total, free, available;
    int used;

    fd = fopen("/proc/meminfo", "r");
    fscanf(fd, "MemTotal: %ld kB\n", &total);
    fscanf(fd, "MemFree: %ld kB\n", &free);
    fscanf(fd, "MemAvailable: %ld kB\n", &available);
    fclose(fd);
    used = 100 * (total - available) / total;
    if(used > 80) {
        return snprintf(status, size, "\x08""M\x05%d%%", used);
    } else {
        return snprintf(status, size, "\x08""M\x02%d%%", used);
    }
}

int getdatetime(char *status, size_t size) {
    time_t result;
    struct tm *resulttm;

    result = time(NULL);
    resulttm = localtime(&result);

    return strftime(status, size, "\x08""D\x02%b %d %H:%M", resulttm);
}

int getbattery(char *status, size_t size) {
    FILE *fd;
    int now, full, bat;
    char stat[12];

    fd = fopen("/sys/class/power_supply/BAT0/charge_now", "r");
    fscanf(fd, "%d", &now);
    fclose(fd);

    fd = fopen("/sys/class/power_supply/BAT0/charge_full", "r");
    fscanf(fd, "%d", &full);
    fclose(fd);

    fd = fopen("/sys/class/power_supply/BAT0/status", "r");
    fscanf(fd, "%s", stat);
    fclose(fd);

    bat = 100 * now / full;

    if(strncmp(stat, "Discharging", 11) == 0) {
        if(bat < 20) {
            return snprintf(status, size, "\x08""B\x05%d%%", bat);
        } else if(bat > 80) {
            return snprintf(status, size, "\x08""B\x06%d%%", bat);
        } else {
            return snprintf(status, size, "\x08""B\x07%d%%", bat);
        }
    } else if(strncmp(stat, "Charging", 8) == 0) {
        return snprintf(status, size, "\x08""B\x06""AC\x02%d%%", bat);
    } else {
        return snprintf(status, size, "\x08""B\x02""AC");
    }
}

int gettemp(char *status, size_t size) {
    int cpu, gpu;
    char *colorc, *colorg;

    cpu = readInt("/sys/class/hwmon/hwmon1/temp1_input") / 1000;
    gpu = readInt("/sys/class/hwmon/hwmon0/temp1_input") / 1000;

    if (cpu > 60)
        colorc = "\x05";
    else if(cpu > 45)
        colorc = "\x07";
    else
        colorc = "\x02";

    if (gpu > 90)
        colorg = "\x05";
    else if (gpu > 70)
        colorg = "\x07";
    else
        colorg = "\x02";

    return snprintf(status, size, "\x08""T%s%d°C%s%d°C", colorc, cpu, colorg, gpu);
}

int getvol(char* status, size_t size) {
    int vol;

    vol = readInt("/tmp/volume");

    if(vol == 0) {
        return snprintf(status, size, "\x08""V\x05""Mute");
    } else {
        return snprintf(status, size, "\x08""V\x02%d%%", vol);
    }
}

int getnetwork(char* status, size_t size) {
    FILE *fd;
    char net[20];

    fd = fopen("/tmp/network-profile", "r");
    if (fd==NULL) {
        return 0;
    }
    fscanf(fd, "%s", net);
    fclose(fd);

    return snprintf(status, size, "\x08""W\x02%s", net);
}

int main(void) {
    char status[100];
    int l = 0;

    if (!(dpy = XOpenDisplay(NULL))) {
        fprintf(stderr, "Cannot open display.\n");
        return 1;
    }

    for (;;sleep(2)) {

        l = getcpu(status, sizeof(status));
        l += gettemp(status + l, sizeof(status) - l);
        l += getmem(status + l, sizeof(status) - l);
        l += getnetwork(status + l, sizeof(status) - l);
        l += getbattery(status + l, sizeof(status) - l);
        l += getvol(status + l, sizeof(status) - l);
        l += getdatetime(status + l, sizeof(status) - l);

        setstatus(status);
    }

    free(status);
    XCloseDisplay(dpy);

    return 0;
}

