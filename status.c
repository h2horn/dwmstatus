/* made by profil 2011-12-29.
**
** Compile with:
** gcc -Wall -pedantic -std=c99 -lX11 status.c
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
    work0 = jif1 + jif2 + jif3;
    total0 = work0 + jif4 + jif5 + jif6 + jif7;

    c = 0;
    while (c != '\n') c = fgetc(fd);
    fscanf(fd, "cpu1 %ld %ld %ld %ld %ld %ld %ld", &jif1, &jif2, &jif3, &jif4, &jif5, &jif6, &jif7);
    work1 = jif1 + jif2 + jif3;
    total1 = work1 + jif4 + jif5 + jif6 + jif7;

    fclose(fd);

    load0 = 100 * (work0 - cpu0_work) / (total0 - cpu0_total);
    load1 = 100 * (work1 - cpu1_work) / (total1 - cpu1_total);
    
    cpu0_work = work0;
    cpu0_total = total0;
    cpu1_work = work1;
    cpu1_total = total1;

    // ---- FREQ
    fd = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", "r");
    fscanf(fd, "%d", &freq);
    fclose(fd);
    if(freq <= 800000) {
        color0 = "\x02";
    } else if(freq >= 2000000) {
        color0 = "\x03";
    } else {
        color0 = "\x05";
    }

    fd = fopen("/sys/devices/system/cpu/cpu1/cpufreq/scaling_cur_freq", "r");
    fscanf(fd, "%d", &freq);
    fclose(fd);
    if(freq <= 800000) {
        color1 = "\x02";
    } else if(freq >= 2000000) {
        color1 = "\x03";
    } else {
        color1 = "\x05";
    }

    return snprintf(status, size, "\x02\uE026 %s%d%% %s%d%%", color0, load0, color1, load1);
}

int getmem(char *status, size_t size) {
    FILE *fd;
    long total, free, buf, cache;
    int used;

    fd = fopen("/proc/meminfo", "r");
    fscanf(fd, "MemTotal: %ld kB\nMemFree: %ld kB\nBuffers: %ld kB\nCached: %ld kB\n", &total, &free, &buf, &cache);
    fclose(fd);
    used = 100 * (total - free - buf - cache) / total;
    if(used > 80) {
        return snprintf(status, size, "\x02\uE021 \x03%d%%", used);
    } else {

        return snprintf(status, size, "\x02\uE021 %d%%", used);
    }
}

int getdatetime(char *status, size_t size) {
	time_t result;
	struct tm *resulttm;

	result = time(NULL);
	resulttm = localtime(&result);
	
    return strftime(status, size, "\x02\uE016 %a %b %d %H:%M", resulttm);
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
            return snprintf(status, size, "\x03\uE03D %d%%", bat);
        } else if(bat > 80) {
            return snprintf(status, size, "\x04\uE03F %d%%", bat);
        } else {
            return snprintf(status, size, "\x05\uE03E %d%%", bat);
        }
    } else if(strncmp(stat, "Charging", 8) == 0) {
        return snprintf(status, size, "\x02\uE043 %d%%", bat);
    } else {
        return snprintf(status, size, "\x02\uE043");
    }
}

int gettemp(char *status, size_t size) {
    FILE *fd;
    int temp;

    fd = fopen("/sys/class/hwmon/hwmon0/temp1_input", "r");
    fscanf(fd, "%d", &temp);
    fclose(fd);
    temp /= 1000;

    if(temp > 60) {
        return snprintf(status, size, "\x02\uE0CF \x03%d°C", temp);
    } else if(temp > 40) {
        return snprintf(status, size, "\x02\uE0CF \x05%d°C", temp);
    } else {
        return snprintf(status, size, "\x02\uE0CF %d°C", temp);
    }
}

int main(void) {
	char status[200];
    int l = 0;

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "Cannot open display.\n");
		return 1;
	}

	for (;;sleep(2)) {
        
        l = getcpu(status, sizeof(status));
        l += snprintf(status + l, sizeof(status) - l, "\x01 | ");
        l += gettemp(status + l, sizeof(status) - l);
        l += snprintf(status + l, sizeof(status) - l, "\x01 | ");
        l += getmem(status + l, sizeof(status) - l);
        l += snprintf(status + l, sizeof(status) - l, "\x01 | ");
        l += getbattery(status + l, sizeof(status) - l);
        l += snprintf(status + l, sizeof(status) - l, "\x01 | ");
        l += getdatetime(status + l, sizeof(status) - l);

		setstatus(status);
	}

	free(status);
	XCloseDisplay(dpy);

	return 0;
}

