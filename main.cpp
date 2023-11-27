#include <string.h>

#include <iostream>
using namespace std;

int main() {
    char text[128] = "GET /root/local/ HTTP/1.1";
    char *m_url = strpbrk(text, " \t");
    if (!m_url) {
        cout << "BAD parse" << endl;
        return 0;
    }
    *m_url++ = '\0';
    cout << m_url << endl;
    // printf("%s\n", m_url);
    cout << strspn(m_url, " \t") << endl;

    // cout << text << endl;
    // int ret = strcasecmp(text, "GET");
    // cout << ret << endl;
    return 0;
}