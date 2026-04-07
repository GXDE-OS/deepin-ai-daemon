// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "resourcemanager.h"

#include <unistd.h>
#include <fcntl.h>
#include <mutex>
#include <malloc.h>
#include <QLoggingCategory>

static constexpr int kMemoryThreshold { 50 * 1024 };   // 50MB
static constexpr int kTimerInterval { 60 * 1000 };   // 1 min

Q_DECLARE_LOGGING_CATEGORY(logUtils)

ResourceManager::ResourceManager(QObject *parent)
    : QObject(parent)
{
    init();
}

void ResourceManager::init()
{
    qCDebug(logUtils) << "Initializing resource manager with timer interval:" << kTimerInterval << "ms";
    autoTimer.setInterval(kTimerInterval);
    connect(&autoTimer, &QTimer::timeout, this, &ResourceManager::autoReleaseResource);
}

ResourceManager *ResourceManager::instance()
{
    static ResourceManager ins;
    return &ins;
}

void ResourceManager::setAutoReleaseMemory(bool enable)
{
    qCInfo(logUtils) << "Setting auto memory release to:" << enable;
    enableReleaseMem = enable;
    if (enable && !autoTimer.isActive()) {
        qCDebug(logUtils) << "Starting auto memory release timer";
        autoTimer.start();
    }
}

bool ResourceManager::autoReleaseMemory()
{
    return enableReleaseMem;
}

void ResourceManager::autoReleaseResource()
{
    if (enableReleaseMem) {
        qCDebug(logUtils) << "Auto releasing resources";
        releaseMemory();
    }
}

void ResourceManager::releaseMemory()
{
    auto memUsage = getMemoryUsage(getpid());
    qCDebug(logUtils) << "Current memory usage:" << memUsage << "kB, threshold:" << kMemoryThreshold << "kB";
    
    if (memUsage > kMemoryThreshold) {
        qCInfo(logUtils) << "Memory usage exceeds threshold, performing memory trim";
        malloc_trim(0);
    }
}

double ResourceManager::getMemoryUsage(int pid)
{
    double usage { 0 };
    int fd;
    const size_t bsiz = 1024;
    char path[128] {}, buf[bsiz + 1] {};
    ssize_t nr;

    sprintf(path, "/proc/%d/statm", pid);

    // open /proc/[pid]/statm
    if ((fd = open(path, O_RDONLY)) < 0) {
        qCWarning(logUtils) << "Failed to open" << path << "for memory stats";
        return usage;
    }

    nr = read(fd, buf, bsiz);
    close(fd);
    if (nr < 0) {
        qCWarning(logUtils) << "Failed to read memory stats from" << path;
        return usage;
    }

    buf[nr] = '\0';
    unsigned long long vmsize;   // vm size in kB
    unsigned long long rss;   // resident set size in kB
    unsigned long long shm;   // resident shared size in kB
    // get resident set size & resident shared size in pages
    nr = sscanf(buf, "%llu %llu %llu", &vmsize, &rss, &shm);
    if (nr == 3) {
        static unsigned int kbShift = 0;
        static std::once_flag flag;
        std::call_once(flag, [] {
            int shift = 0;
            long size;

            /* One can also use getpagesize() to get the size of a page */
            if ((size = sysconf(_SC_PAGESIZE)) == -1)
                return;

            size >>= 10; /* Assume that a page has a minimum size of 1 kB */
            while (size > 1) {
                shift++;
                size >>= 1;
            }

            kbShift = uint(shift);
        });
        // convert to kB
        rss <<= kbShift;
        shm <<= kbShift;

        usage = rss - shm;
    }

    return usage;
}
