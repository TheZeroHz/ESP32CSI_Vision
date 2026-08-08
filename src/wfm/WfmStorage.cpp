#include "WfmStorage.h"

WfmStorage::WfmStorage() { _mu = xSemaphoreCreateMutex(); }

void WfmStorage::lock() {
  if (_mu) xSemaphoreTake(_mu, portMAX_DELAY);
}
void WfmStorage::unlock() {
  if (_mu) xSemaphoreGive(_mu);
}

bool WfmStorage::exists(const char *path) {
  if (!mounted() || !path) return false;
  lock();
  bool ok = fs().exists(path);
  unlock();
  return ok;
}

bool WfmStorage::remove(const char *path) {
  if (!mounted() || !path) return false;
  lock();
  bool ok = fs().remove(path);
  unlock();
  return ok;
}

bool WfmStorage::mkdir(const char *path) {
  if (!mounted() || !path) return false;
  lock();
  bool ok = fs().mkdir(path);
  unlock();
  return ok;
}

bool WfmStorage::rename(const char *from, const char *to) {
  if (!mounted() || !from || !to) return false;
  lock();
  bool ok = fs().rename(from, to);
  unlock();
  return ok;
}
