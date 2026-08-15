
__attribute__((externally_visible))
const char *
__asan_default_options()
{
#ifdef PLATFORM_MIYOOMINI
    return "log_path=/mnt/SDCARD/.tmp_update/logs/ASAN.log:halt_on_error=0";
#else
    return "halt_on_error=1";
#endif
}
