#include <pspkernel.h>
#include <pspsdk.h>
#include <pspiofilemgr.h>
#include <string.h>

PSP_MODULE_INFO("MKHybridMod", 0, 1, 0);

void WriteLog(const char *msg) {
    SceUID fd = sceIoOpen("ms0:/PSP/PLUGINS/mkswap/log.txt", PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
    if (fd >= 0) {
        sceIoWrite(fd, msg, strlen(msg));
        sceIoClose(fd);
    }
}

void PatchShujinkoToScorpion(void) {
    unsigned int start_addr = 0x08800000;
    unsigned int end_addr   = 0x09600000;
    int patched = 0;

    for (unsigned int addr = start_addr; addr < end_addr; addr += 4) {
        char *ptr = (char*)addr;

        // 1. استبدال مجسم الجسم والرأس (13 حرفاً = 13 حرفاً)
        if (memcmp(ptr, "BODY_SHUJINKO", 13) == 0) {
            memcpy(ptr, "BODY_SCORPION", 13);
            patched++;
        }
        else if (memcmp(ptr, "HEAD_SHUJINKO", 13) == 0) {
            memcpy(ptr, "HEAD_SCORPION", 13);
            patched++;
        }
        // 2. استبدال الزي البديل (21 حرفاً = 21 حرفاً)
        else if (memcmp(ptr, "body_shujinko_alt.sec", 21) == 0) {
            memcpy(ptr, "body_scorpion_alt.sec", 21);
            patched++;
        }
        // 3. استبدال أساليب الضربات العادية لشوجينكو (MANTIS / SHAOLIN FIST / DAN TIEN DAO)
        // بأساليب ضربات سكوربيون العادية (HAPKIDO / MOI FAH / MUGAI RYU)
        else if (memcmp(ptr, "MANTIS", 7) == 0) {
            memcpy(ptr, "HAPKIDO", 8);
            patched++;
        }
        else if (memcmp(ptr, "SHAOLIN FIST", 13) == 0) {
            memcpy(ptr, "MOI FAH\0\0\0\0\0", 13);
            patched++;
        }
        else if (memcmp(ptr, "DAN TIEN DAO", 13) == 0) {
            memcpy(ptr, "MUGAI RYU\0\0\0", 13);
            patched++;
        }
        // ملاحظة: نترك "shujinko.mko" كما هو دون تغيير ليبقى محتفظاً بالقدرات الخارقة (كرة الثلج، الخطاف...)
    }

    if (patched > 0) {
        sceKernelDcacheWritebackInvalidateAll();
        WriteLog("INFO: Shujinko patched to Scorpion body & normal attacks successfully!\r\n");
    }
}

int MainThread(SceSize args, void *argp) {
    WriteLog("INFO: MKHybridMod initialized!\r\n");

    // مسح الذاكرة على دفعات أثناء تحميل اللعبة لضمان التقاط الجدول قبل بدء القتال
    for (int i = 0; i < 15; i++) {
        PatchShujinkoToScorpion();
        sceKernelDelayThread(500000); // كل نصف ثانية لمدة 7.5 ثوانٍ
    }

    return sceKernelExitDeleteThread(0);
}

int module_start(SceSize args, void *argp) {
    SceUID thid = sceKernelCreateThread("MKHybridThread", MainThread, 0x18, 0x4000, 0, NULL);
    if (thid >= 0) {
        sceKernelStartThread(thid, args, argp);
    }
    return 0;
}

int module_stop(SceSize args, void *argp) {
    return 0;
}
