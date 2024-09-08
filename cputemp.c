// This program is in public domain. Written by Naoki Shibata  https://github.com/shibatch

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>

int setFreq(double d) {
  int c = (int)(d * (1.0 / 1000));
  for(int i=0;;i++) {
    char fn[256];
    snprintf(fn, sizeof(fn), "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_max_freq", i);
    FILE *fp = fopen(fn, "w");
    if (fp == NULL) return i;
    fprintf(fp, "%d", c);
    fclose(fp);
  }
}

int readNumber(char *fn) {
  FILE *fp = fopen(fn, "r");
  if (fp == NULL) return INT_MIN;
  int ret = INT_MIN;
  if (fscanf(fp, "%d", &ret) != 1) ret = INT_MIN;
  fclose(fp);
  return ret;
}

int readString(char *buf, size_t n, char *fn) {
  if (n == 0) return 0;
  buf[0] = '\0';
  FILE *fp = fopen(fn, "r");
  if (fp == NULL) return 0;
  int ret = 1;
  if (fgets(buf, n, fp) == NULL) ret = 0;
  fclose(fp);
  if (buf[0] != '\0') buf[strlen(buf)-1] = '\0';
  return ret;
}

static char coreTempFn[1024] = { '\0' };

void findCoreTempFn(const char *sensorID) {
  int maxTemp = -1;
  char fn[1000], str[1000];

  for(int i=0;i<10;i++) {
    snprintf(fn, sizeof(fn), "/sys/class/thermal/thermal_zone%d/type", i);
    if (!readString(str, sizeof(str), fn)) continue;
    if (strcmp(str, sensorID) != 0) continue;

    snprintf(fn, sizeof(fn), "/sys/class/thermal/thermal_zone%d/temp", i);
    int t = readNumber(fn);
    if (t > maxTemp) {
      maxTemp = t;
      strncpy(coreTempFn, fn, sizeof(coreTempFn));
    }
  }

  for(int i=0;i<10;i++) {
    snprintf(fn, sizeof(fn), "/sys/class/hwmon/hwmon%d/name", i);
    if (!readString(str, sizeof(str), fn)) continue;
    if (strcmp(str, sensorID) != 0) continue;

    snprintf(fn, sizeof(fn), "/sys/class/hwmon/hwmon%d/temp1_input", i);
    int t = readNumber(fn);
    if (t > maxTemp) {
      maxTemp = t;
      strncpy(coreTempFn, fn, sizeof(coreTempFn));
    }
  }
}

double getTemp() {
  int t = readNumber(coreTempFn);
  if (t == -1) abort();
  return t * (1.0 / 1000);
}
double getMinFreq() { return readNumber("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_min_freq") * 1000.0; }
double getMaxFreq() { return readNumber("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq") * 1000.0; }
double getFreq()    { return readNumber("/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq") * 1000.0; }

int main(int argc, char **argv) {
  if (argc == 1) {
    fprintf(stderr, "This program controls the CPU frequency to make its temperature close to the target\n");
    fprintf(stderr, "Usage : %s <sensor ID> <target temperature in celcius> <interval in second> [verbose]\n", argv[0]);
    fprintf(stderr, "\nhttps://github.com/shibatch\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Sensor IDs : ");
    char fn[1000], str[1000];
    for(int i=0;i<10;i++) {
      snprintf(fn, sizeof(fn), "/sys/class/hwmon/hwmon%d/name", i);
      if (readString(str, sizeof(str), fn)) {
	snprintf(fn, sizeof(fn), "/sys/class/hwmon/hwmon%d/temp1_input", i);
	int t = readNumber(fn);
	if (t != INT_MIN) fprintf(stderr, "%s (%d), ", str, t);
      }
    }
    for(int i=0;i<10;i++) {
      snprintf(fn, sizeof(fn), "/sys/class/thermal/thermal_zone%d/type", i);
      if (readString(str, sizeof(str), fn)) {
	snprintf(fn, sizeof(fn), "/sys/class/thermal/thermal_zone%d/temp", i);
	int t = readNumber(fn);
	if (t != INT_MIN) fprintf(stderr, "%s (%d), ", str, t);
      }
    }
    fprintf(stderr, "\n");
    exit(-1);
  }

  const char *sensorID = argv[1];
  double targetTemp = -1;
  double interval = 1.0;
  int verbose = 0;

  if (argc >= 3) targetTemp = atof(argv[2]);
  if (argc >= 4) interval = atof(argv[3]);
  if (argc >= 5) verbose = 1;

  findCoreTempFn(sensorID);

  if (targetTemp < 0) {
    int t = readNumber(coreTempFn);
    if (t == INT_MIN) {
      fprintf(stderr, "Could not find sensor %s\n", sensorID);
      exit(-1);
    }
    printf("%d\n", t);
    exit(0);
  }

  if (coreTempFn[0] == '\0') {
    fprintf(stderr, "Could not find sensor %s\n", sensorID);
    exit(-1);
  }

  if (interval <= 0) {
    fprintf(stderr, "Interval must be positive\n");
    exit(-1);
  }

  if (verbose) {
    printf("Sensor file name : %s\n", coreTempFn);
    printf("Max freq : %g MHz\n", getMaxFreq() / 1000.0 / 1000.0);
    printf("Min freq : %g MHz\n", getMinFreq() / 1000.0 / 1000.0);
  }

  double prevTemp = -1;
  double C1 = 20000000.0 * interval, C2 = 20000000.0 * interval;

  double curFreq = getFreq();
  double maxFreq = getMaxFreq(), minFreq = getMinFreq();

  for(;;) {
    double curTemp = getTemp();
    if (prevTemp == -1) prevTemp = curTemp;

    if (verbose) printf("Clock = %dMHz, temp = %.3g, target = %.3g\n", (int)(curFreq / 1e+6), curTemp, targetTemp);

    double nextFreq = curFreq + C1 * (targetTemp - curTemp) - C2 * (curTemp - prevTemp);
    if (nextFreq > maxFreq) nextFreq = maxFreq;
    if (nextFreq < minFreq) nextFreq = minFreq;

    setFreq(nextFreq);

    curFreq = nextFreq;
    prevTemp = curTemp;

    usleep((useconds_t)(interval * 1000000.0));
  }
}
