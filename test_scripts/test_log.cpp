#include "../log/log.h"
#include <unistd.h>
int main()
{
    int log_close = 0;
    Log::get_instance()->init("/home/ubuntu/myTinyWebserver/log_txt/logtest", 0, 60, 800, 20);
    LOG_DEBUG("debug test");
    LOG_INFO("%d, %s\n", 22, "abc");
    sleep(1);
    return 0;
}
