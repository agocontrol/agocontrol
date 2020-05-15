#include <boost/log/support/date_time.hpp>

#include <proc/readproc.h>
#include "agosystem.h"

/**
 * Return total CPU time from /proc/stat file
 *
 * @return 0 if error occured
 */
static uint32_t getCpuTotalTime()
{
    uint32_t cpuTotalTime = 0;
    int i = 0;

    FILE *fstat = fopen("/proc/stat", "r");
    if (fstat == NULL)
    {
        perror("FOPEN ERROR ");
        fclose(fstat);
        return 0;
    }
    long unsigned int cpu_time[10];
    bzero(cpu_time, sizeof(cpu_time));
    if (fscanf(fstat, "%*s %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
                &cpu_time[0], &cpu_time[1], &cpu_time[2], &cpu_time[3],
                &cpu_time[4], &cpu_time[5], &cpu_time[6], &cpu_time[7],
                &cpu_time[8], &cpu_time[9]) == EOF)
    {
        fclose(fstat);
        return 0;
    }
    fclose(fstat);

    for(i=0; i < 10;i++)
    {
        cpuTotalTime += cpu_time[i];
    }

    return cpuTotalTime;
}

/**
 * Calculates the elapsed CPU percentage between 2 measuring points.
 * CPU usage calculation extracted from https://github.com/fho/code_snippets/blob/master/c/getusage.c
 */
static void getCpuPercentage(Json::Value& current, Json::Value& last, double* ucpu, double* scpu)
{
    uint32_t totalTimeDiff = current["cpuTotalTime"].asUInt() - last["cpuTotalTime"].asUInt();
    *ucpu = 100 * (double)(((current["utime"].asUInt64() + current["cutime"].asUInt64()) - (last["utime"].asUInt64() + last["cutime"].asUInt64())) / (double)totalTimeDiff);
    *scpu = 100 * (double)(((current["stime"].asUInt64() + current["cstime"].asUInt64()) - (last["stime"].asUInt64() + last["cstime"].asUInt64())) / (double)totalTimeDiff);
    //fix wrong cpu usage if process restarted
    if( *ucpu<0 )
    {
        *ucpu = 0;
    }
    if( *scpu<0 )
    {
        *scpu = 0;
    }
}

void AgoSystem::getProcessInfo()
{
    PROCTAB* proc = openproc(PROC_FILLMEM | PROC_FILLSTAT | PROC_FILLSTATUS);
#ifdef FREEPROC_EXISTS
    proc_t* proc_info;
    while( (proc_info=readproc(proc, NULL)) != NULL )
#else
    proc_t proc_info;
    memset(&proc_info, 0, sizeof(proc_info));
    while (readproc(proc, &proc_info) != NULL)
#endif
    {
#ifdef FREEPROC_EXISTS
        std::string procName = std::string(proc_info->cmd);
#else
        std::string procName = std::string(proc_info.cmd);
#endif
        if(processes.isMember(procName))
        {
            Json::Value stats = processes[procName];
            Json::Value cs = stats["currentStats"];
            Json::Value ls = stats["lastStats"];

            //update current stats
#ifdef FREEPROC_EXISTS
            cs["utime"] = (uint64_t)proc_info->utime;
            cs["stime"] = (uint64_t)proc_info->stime;
            cs["cutime"] = (uint64_t)proc_info->cutime;
            cs["cstime"] = (uint64_t)proc_info->cstime;
            cs["vsize"] = (uint64_t)proc_info->vm_size * 1024;
            cs["rss"] = (uint64_t)proc_info->vm_rss * 1024;
#else
            cs["utime"] = (uint64_t)proc_info.utime;
            cs["stime"] = (uint64_t)proc_info.stime;
            cs["cutime"] = (uint64_t)proc_info.cutime;
            cs["cstime"] = (uint64_t)proc_info.cstime;
            cs["vsize"] = (uint64_t)proc_info.vm_size * 1024;
            cs["rss"] = (uint64_t)proc_info.vm_rss * 1024;
#endif
            cs["cpuTotalTime"] = (uint32_t)getCpuTotalTime();
            double ucpu=0, scpu=0;
            if( ls["utime"].asUInt64()!=0 )
            {
                getCpuPercentage(cs, ls, &ucpu, &scpu);
            }
            cs["ucpu"] = (uint64_t)ucpu;
            cs["scpu"] = (uint64_t)scpu;

            //update last stats
            stats["lastStats"] = cs;

            //finalize
            stats["currentStats"] = cs;
            stats["running"] = true;
            processes[procName] = stats;
        }
#ifdef FREEPROC_EXISTS
        freeproc(proc_info);
#endif
    }
    closeproc(proc);
}

