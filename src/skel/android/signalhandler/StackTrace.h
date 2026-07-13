//
// Created by mrxenginner on 13/07/2025.
//

#ifndef REVC_STACKTRACE_H
#define REVC_STACKTRACE_H

#if defined ANDROID

#include <dlfcn.h>
#include <execinfo.h>
#include <unwind.h>

extern uintptr_t g_libREVC;

#ifdef __arm__
#define PRINT_CRASH_STATES(context) \
	Logger::CrashLog("register states:"); \
	Logger::CrashLog("r0: 0x%X, r1: 0x%X, r2: 0x%X, r3: 0x%X", (context)->uc_mcontext.arm_r0, (context)->uc_mcontext.arm_r1, (context)->uc_mcontext.arm_r2, (context)->uc_mcontext.arm_r3); \
	Logger::CrashLog("r4: 0x%x, r5: 0x%x, r6: 0x%x, r7: 0x%x", (context)->uc_mcontext.arm_r4, (context)->uc_mcontext.arm_r5, (context)->uc_mcontext.arm_r6, (context)->uc_mcontext.arm_r7); \
	Logger::CrashLog("r8: 0x%x, r9: 0x%x, sl: 0x%x, fp: 0x%x", (context)->uc_mcontext.arm_r8, (context)->uc_mcontext.arm_r9, (context)->uc_mcontext.arm_r10, (context)->uc_mcontext.arm_fp); \
	Logger::CrashLog("ip: 0x%x, sp: 0x%x, lr: 0x%x, pc: 0x%x", (context)->uc_mcontext.arm_ip, (context)->uc_mcontext.arm_sp, (context)->uc_mcontext.arm_lr, (context)->uc_mcontext.arm_pc); \
    Logger::CrashLog("1: libreVC.so + 0x%X", context->uc_mcontext.arm_pc - g_libREVC); \
    Logger::CrashLog("2: libreVC.so + 0x%X", context->uc_mcontext.arm_lr - g_libREVC);
#else
#define PRINT_CRASH_STATES(context) \
    Logger::CrashLog("1: libreVC.so + 0x%llx", context->uc_mcontext.pc - g_libREVC); \
    Logger::CrashLog("2: libreVC.so + 0x%llx", context->uc_mcontext.regs[30] - g_libREVC);
#endif

class CStackTrace
{
public:
    static void printBacktrace()
    {
        Logger::CrashLog("------------ START BACKTRACE ------------");
        Logger::CrashLog(" ");
        PrintStackTrace();
    }

private:
    static _Unwind_Reason_Code TraceFunction(_Unwind_Context* context, void* arg) {
        uintptr_t pc = _Unwind_GetIP(context);

        Dl_info info;
        if (dladdr(reinterpret_cast<void*>(pc), &info) && info.dli_sname != nullptr) {
            Logger::CrashLog("[adr: %p reVC: %p] %s\n",
                     reinterpret_cast<void*>(pc),
                     reinterpret_cast<void*>(pc - g_libREVC),
                     info.dli_sname);
        } else {
            Logger::CrashLog("[adr: %p reVC: %p] name not found\n",
                     reinterpret_cast<void*>(pc),
                     reinterpret_cast<void*>(pc - g_libREVC));
        }

        return _URC_NO_REASON;
    }

    static void PrintStackTrace() {
        _Unwind_Backtrace(TraceFunction, nullptr);
    }

};
#endif

#endif