// This program is in public domain. Written by Naoki Shibata  https://github.com/shibatch
// This is a simple alternative to thermald

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int setFreq(double d) {
  int c = (int)(d * (1.0 / 1000));
  for(int i=0;;i++) {
    char fn[256];
    sprintf(fn, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_max_freq", i);
    FILE *fp = fopen(fn, "w");
    if (fp == NULL) return i;
    fprintf(fp, "%d", c);
    fclose(fp);
  }
}

int readNumber(char *fn) {
  FILE *fp = fopen(fn, "r");
  if (fp == NULL) abort();
  int ret;
  if (fscanf(fp, "%d", &ret) != 1) abort();
  fclose(fp);
  return ret;
}

static char coreTempFn[1024];

void findCoreTempFn() {
  for(int i=0;i<8;i++) {
    snprintf(coreTempFn, 1000, "/sys/class/thermal/thermal_zone%d/temp", i);
    FILE *fp = fopen(coreTempFn, "r");
    if (fp != NULL) {
      fclose(fp);
      return;
    }
  }
  for(int i=0;i<8;i++) {
    for(int j=0;j<8;j++) {
      for(int k=0;k<16;k++) {
	snprintf(coreTempFn, 1000, "/sys/bus/platform/devices/coretemp.%d/hwmon/hwmon%d/temp%d_input", i, j, k);
	FILE *fp = fopen(coreTempFn, "r");
	if (fp != NULL) {
	  fclose(fp);
	  return;
	}
      }
    }
  }
  exit(-1);
}

double getTemp()    { return readNumber(coreTempFn) * (1.0 / 1000); }
double getMinFreq() { return readNumber("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_min_freq") * 1000.0; }
double getMaxFreq() { return readNumber("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq") * 1000.0; }
double getFreq()    { return readNumber("/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq") * 1000.0; }

int main(int argc, char **argv) {
  if (argc == 1) {
    fprintf(stderr, "This program controls the CPU frequency to make its temperature close to the target\n");
    fprintf(stderr, "Usage : %s <target temp in degrees> [<interval in second>] [-]\n", argv[0]);
    fprintf(stderr, "\nhttps://github.com/shibatch\n");
    exit(-1);
  }

  findCoreTempFn();

  double targetTemp = 70.0;
  double interval = 10.0;
  if (argc > 1) targetTemp = (int)(atof(argv[1]));
  if (argc > 2) interval   = atof(argv[2]);
  int verbose = argc > 3;

  if (verbose) printf("Coretemp file name : %s\n", coreTempFn);

  double prevTemp = -1;
  double C1 = 2000000.0 * interval, C2 = 2000000.0 * interval;

  double curFreq = getFreq();
  double maxFreq = getMaxFreq(), minFreq = getMinFreq();

  for(;;) {
    double curTemp = getTemp();
    if (prevTemp == -1) prevTemp = curTemp;

    if (verbose) printf("clock = %dMHz, temp = %.2g, target = %.2g\n", (int)(curFreq / 1e+6), curTemp, targetTemp);

    double nextFreq = curFreq + C1 * (targetTemp - curTemp) - C2 * (curTemp - prevTemp);
    if (nextFreq > maxFreq) nextFreq = maxFreq;
    if (nextFreq < minFreq) nextFreq = minFreq;

    setFreq(nextFreq);

    curFreq = nextFreq;
    prevTemp = curTemp;

    usleep((useconds_t)(interval * 1000000.0));
  }
}
