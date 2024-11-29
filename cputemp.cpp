#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <thread>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <climits>
#include <float.h>
#include <math.h>
#include <bsd/libutil.h>
#include <signal.h>

using namespace std;

int setFreq(double f) {
  char fn[256];
  for(int i=0;;i++) {
    snprintf(fn, sizeof(fn), "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_max_freq", i);
    FILE *fp = fopen(fn, "w");
    if (fp == NULL) return i;
    fprintf(fp, "%d", (int)(f * (1.0 / 1000)));
    fclose(fp);
  }
}

long readNumber(const string &fn) {
  long ret = LONG_MIN;
  FILE *fp = fopen(fn.c_str(), "r");
  if (fp) {
    if (fscanf(fp, "%ld", &ret) != 1) ret = LONG_MIN;
    fclose(fp);
  }
  return ret;
}

string readString(const string &fn) {
  char buf[1024] = { '\0' };
  FILE *fp = fopen(fn.c_str(), "r");
  if (fp) {
    if (fgets(buf, sizeof(buf), fp)) {
      fclose(fp);
      string s(buf);
      return s.substr(0, s.size()-1);
    }
    fclose(fp);
  }
  return "";
}

string temperatureFn;

void findTemperatureFn(const string &sensorID) {
  for(int i=0;;i++) {
    string type = readString(string("/sys/class/thermal/thermal_zone") + to_string(i) + "/type");
    if (type == "") break;
    if (type != sensorID) continue;

    string fn = string("/sys/class/thermal/thermal_zone") + to_string(i) + "/temp";
    if (readNumber(fn) != LONG_MIN) {
      temperatureFn = fn;
      return;
    }
  }

  for(int i=0;;i++) {
    string name = readString(string("/sys/class/hwmon/hwmon" + to_string(i) + "/name"));
    if (name == "") break;
    if (name != sensorID) continue;

    string fn = string("/sys/class/hwmon/hwmon") + to_string(i) + "/temp1_input";
    if (readNumber(fn) != LONG_MIN) {
      temperatureFn = fn;
      return;
    }
  }

  cerr << "Could not find sensor " << sensorID << endl;
  exit(-1);
}

pair<double, double> getMinMaxFreq(const string &fn) {
  double max = -DBL_MAX, min = DBL_MAX;
  for(int i=0;;i++) {
    long t = readNumber(string("/sys/devices/system/cpu/cpu") + to_string(i) + "/cpufreq/" + fn); 
    if (t == LONG_MIN) break;
    min = fmin(min, t);
    max = fmax(max, t);
  }
  return pair<double, double>(min * 1000.0, max * 1000.0);
}

double readTemp() { return readNumber(temperatureFn) * (1.0 / 1000); }
double getMinFreq() { return getMinMaxFreq("cpuinfo_min_freq").first; }
double getMaxFreq() { return getMinMaxFreq("cpuinfo_max_freq").second; }
double getFreq() { return getMinMaxFreq("scaling_max_freq").second; }
double getCurFreq() { return getMinMaxFreq("scaling_cur_freq").second; }

struct pidfh *pfh = NULL;

void handler(int n) {
  if (pfh) pidfile_remove(pfh);
  setFreq(getMaxFreq());
  exit(0);
}

void showUsage(const string& argv0, const string& mes = "") {
  if (mes != "") cerr << mes << endl << endl;
  cerr << "Usage : " << argv0 << " [<options>]" << endl;
  cerr << endl;
  cerr << "This utility controls the CPU frequency to make its temperature close to the target" << endl;
  cerr << endl;
  cerr << "Options :" << endl;
  cerr << "  --sensor <sensor name>         Specify sensor name" << endl;
  cerr << "  --period <seconds>             Specify period" << endl;
  cerr << "  --temp <target temperature>    Specify target CPU temperature" << endl;
  cerr << "  --daemon <pid file name>       Daemonize" << endl;
  cerr << "  --kill-daemon <pid file name>  Kill already running daemon" << endl;
  cerr << "  --verbose                      Turn on verbose mode" << endl;
  cerr << endl;
  cerr << "Available sensors : ";

  for(int i=0;;i++) {
    string type = readString(string("/sys/class/thermal/thermal_zone") + to_string(i) + "/type");
    if (type == "") break;
    long t = readNumber(string("/sys/class/thermal/thermal_zone") + to_string(i) + "/temp");
    if (t != LONG_MIN) cerr << type << " ( " << (t * (1.0 / 1000)) << " C), ";
  }

  for(int i=0;;i++) {
    string name = readString(string("/sys/class/hwmon/hwmon" + to_string(i) + "/name"));
    if (name == "") break;
    long t = readNumber(string("/sys/class/hwmon/hwmon") + to_string(i) + "/temp1_input");
    if (t != LONG_MIN) cerr << name << " (" << (t * (1.0 / 1000)) << " C), ";
  }

  cerr << endl;
  exit(-1);
}

int main(int argc, char **argv) {
  if (argc < 2) showUsage(argv[0], "");

  string sensorID = "", pidfn = "";
  double period = 1.0, targetTemp = 80;
  bool verbose = false, killDaemon = false;

  int nextArg;
  for(nextArg = 1;nextArg < argc;nextArg++) {
    if (string(argv[nextArg]) == "--sensor") {
      if (nextArg+1 >= argc) showUsage(argv[0]);
      sensorID = argv[nextArg+1];
      nextArg++;
    } else if (string(argv[nextArg]) == "--period") {
      if (nextArg+1 >= argc) showUsage(argv[0]);
      char *p;
      period = strtod(argv[nextArg+1], &p);
      if (p == argv[nextArg+1] || *p || period <= 0)
	showUsage(argv[0], "A positive real value is expected after --period.");
      nextArg++;
    } else if (string(argv[nextArg]) == "--temp") {
      if (nextArg+1 >= argc) showUsage(argv[0]);
      char *p;
      targetTemp = strtod(argv[nextArg+1], &p);
      if (p == argv[nextArg+1] || *p)
	showUsage(argv[0], "A real value is expected after --temp.");
      nextArg++;
    } else if (string(argv[nextArg]) == "--verbose") {
      verbose = true;
    } else if (string(argv[nextArg]) == "--daemon") {
      if (nextArg+1 >= argc) showUsage(argv[0], "Specify pid file name after --daemon");
      pidfn = argv[nextArg+1];
      nextArg++;
    } else if (string(argv[nextArg]) == "--kill-daemon") {
      if (nextArg+1 >= argc) showUsage(argv[0], "Specify pid file name after --kill-daemon");
      pidfn = argv[nextArg+1];
      killDaemon = true;
      nextArg++;
    } else if (string(argv[nextArg]).substr(0, 2) == "--") {
      showUsage(argv[0], string("Unrecognized option : ") + argv[nextArg]);
    } else {
      break;
    }
  }

  if (pidfn != "" && verbose) 
    showUsage(argv[0], "--daemon and --verbose cannot be specified at a time");

  //

  if (pidfn != "") {
    pid_t otherpid = 0;
    for(int i=0;i<10 && !pfh;i++) {
      pfh = pidfile_open(pidfn.c_str(), 0600, &otherpid);
      if (!pfh && errno == EEXIST) {
	if (otherpid != 0 && otherpid != -1) kill(otherpid, SIGTERM);
	this_thread::sleep_for(chrono::milliseconds(100));
      }
    }
    if (!pfh) {
      if (errno == EEXIST) {
	cerr << "Could not kill already running daemon (PID:" << otherpid << ")" << endl;
      } else {
	cerr << "Could not open pid file" << endl;
      }
      exit(-1);
    }

    if (killDaemon) {
      pidfile_remove(pfh);
      exit(0);
    }

    pidfile_close(pfh);
    pfh = NULL;
  }

  //

  findTemperatureFn(sensorID);

  if (verbose) {
    cout << "Sensor file name : " << temperatureFn << endl;
    cout << "Max freq : " << (getMaxFreq() / 1000000.0) << " MHz" << endl;
    cout << "Min freq : " << (getMinFreq() / 1000000.0) << " MHz" << endl;
    cout << "Cur freq : " << (getCurFreq() / 1000000.0) << " MHz" << endl;
  }

  // Test if we can change scaling_max_freq

  const double maxFreq = getMaxFreq(), minFreq = getMinFreq();

  setFreq(minFreq);
  for(int i=0;i<10 && getFreq() != minFreq;i++)
    this_thread::sleep_for(chrono::milliseconds(100));

  if (getFreq() != minFreq) {
    cerr << "Could not set scaling_max_freq to the min freqency." << endl;
    cerr << "Min freq         : " << (getMinFreq() / 1000000.0) << " MHz" << endl;
    cerr << "scaling_max_freq : " << (getFreq() / 1000000.0) << " MHz" << endl;
    exit(-1);
  }

  setFreq(maxFreq);
  for(int i=0;i<10 && getFreq() != maxFreq;i++)
    this_thread::sleep_for(chrono::milliseconds(100));

  if (getFreq() != maxFreq) {
    cerr << "Could not set scaling_max_freq to the max freqency." << endl;
    cerr << "Max freq         : " << (getMaxFreq() / 1000000.0) << " MHz" << endl;
    cerr << "scaling_max_freq : " << (getFreq() / 1000000.0) << " MHz" << endl;
    exit(-1);
  }

  //

  if (pidfn != "") {
    if (!(pfh = pidfile_open(pidfn.c_str(), 0600, NULL))) {
      cerr << "Could not open pid file (2)" << endl;
      exit(-1);
    }

    if (daemon(0, 0) == -1) {
      cerr << "daemon(0, 0) failed" << endl;
      pidfile_remove(pfh);
      exit(-1);
    }

    signal(SIGTERM, handler);
    pidfile_write(pfh);
  }

  //

  const double C1 = 20000000.0 * period, C2 = 20000000.0 * period;
  double prevTemp = readTemp(), curFreq = getFreq();

  for(;;) {
    double curTemp = readTemp();

    if (verbose) {
      cout << "CPU freq = " << (getCurFreq() / 1000000.0) << " MHz, ";
      cout << "scaling_max_freq = " << (getFreq() / 1000000.0) << " MHz, ";
      cout << "CPU temp = " << curTemp << " C, ";
      cout << "target temp = " << targetTemp << " C" << endl;
    }

    double nextFreq = curFreq + C1 * (targetTemp - curTemp) - C2 * (curTemp - prevTemp);
    if (nextFreq > maxFreq) nextFreq = maxFreq;
    if (nextFreq < minFreq) nextFreq = minFreq;

    setFreq(nextFreq);

    curFreq = nextFreq;
    prevTemp = curTemp;

    this_thread::sleep_for(chrono::microseconds((long)(period * 1e+6)));
  }
}
