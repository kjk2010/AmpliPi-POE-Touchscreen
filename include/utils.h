#include <Arduino.h>
#include <stdint.h>

bool validateIPv4(const char *s)
{
    int len = strlen(s);

    if (len < 7 || len > 15)
        return false;

    char tail[16];
    tail[0] = 0;

    unsigned int d[4];
    int c = sscanf(s, "%3u.%3u.%3u.%3u%s", &d[0], &d[1], &d[2], &d[3], tail);

    if (c != 4 || tail[0])
        return false;

    for (int i = 0; i < 4; i++)
        if (d[i] > 255)
            return false;

    return true;
}



// Function used for processing BMP images
uint16_t read16(fs::File &f)
{
    uint16_t result;
    ((uint8_t *)&result)[0] = f.read(); // LSB
    ((uint8_t *)&result)[1] = f.read(); // MSB
    return result;
}

// Function used for processing BMP images
uint32_t read32(fs::File &f)
{
    uint32_t result;
    ((uint8_t *)&result)[0] = f.read(); // LSB
    ((uint8_t *)&result)[1] = f.read();
    ((uint8_t *)&result)[2] = f.read();
    ((uint8_t *)&result)[3] = f.read(); // MSB
    return result;
}