#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

// ============================================================
// OUR CURRENT SD PIN CONFIG
// ============================================================

#define SD_CS    5
#define SD_SCK   2
#define SD_MISO  19
#define SD_MOSI  15

SPIClass SD_SPI(VSPI);

// ============================================================
// Human-readable size helper
// ============================================================

String formatBytes(uint64_t bytes)
{
    if (bytes < 1024)
        return String((uint32_t)bytes) + " B";

    if (bytes < 1024ULL * 1024ULL)
        return String((float)bytes / 1024.0f, 2) + " KB";

    if (bytes < 1024ULL * 1024ULL * 1024ULL)
        return String((float)bytes / (1024.0f * 1024.0f), 2) + " MB";

    return String(
        (float)bytes / (1024.0f * 1024.0f * 1024.0f),
        2
    ) + " GB";
}

// ============================================================
// Print SD card information
// ============================================================

void printSDInfo()
{
    Serial.println();
    Serial.println("========================================");
    Serial.println("SD CARD INFORMATION");
    Serial.println("========================================");

    uint8_t cardType = SD.cardType();

    Serial.print("Card type : ");

    if (cardType == CARD_NONE)
    {
        Serial.println("NONE");
        return;
    }
    else if (cardType == CARD_MMC)
    {
        Serial.println("MMC");
    }
    else if (cardType == CARD_SD)
    {
        Serial.println("SDSC");
    }
    else if (cardType == CARD_SDHC)
    {
        Serial.println("SDHC / SDXC");
    }
    else
    {
        Serial.println("UNKNOWN");
    }

    uint64_t cardSize = SD.cardSize();
    uint64_t total    = SD.totalBytes();
    uint64_t used     = SD.usedBytes();
    uint64_t free     = total - used;

    Serial.print("Card size : ");
    Serial.println(formatBytes(cardSize));

    Serial.print("Total     : ");
    Serial.println(formatBytes(total));

    Serial.print("Used      : ");
    Serial.println(formatBytes(used));

    Serial.print("Free      : ");
    Serial.println(formatBytes(free));

    Serial.println("========================================");
}

// ============================================================
// Recursive directory listing
// Similar to ls -lR
// ============================================================

void listDirectory(const char *dirname, uint8_t levels)
{
    File root = SD.open(dirname);

    if (!root)
    {
        Serial.print("Failed to open directory: ");
        Serial.println(dirname);
        return;
    }

    if (!root.isDirectory())
    {
        Serial.print("Not a directory: ");
        Serial.println(dirname);
        root.close();
        return;
    }

    Serial.println();
    Serial.print("Directory: ");
    Serial.println(dirname);
    Serial.println("----------------------------------------");

    File file = root.openNextFile();

    while (file)
    {
        if (file.isDirectory())
        {
            Serial.print("[DIR ] ");
            Serial.println(file.name());

            if (levels)
            {
                String path;

                if (strcmp(dirname, "/") == 0)
                {
                    path = "/";
                    path += file.name();
                }
                else
                {
                    path = dirname;
                    path += "/";
                    path += file.name();
                }

                file.close();

                listDirectory(
                    path.c_str(),
                    levels - 1
                );
            }
            else
            {
                file.close();
            }
        }
        else
        {
            Serial.print("[FILE] ");

            Serial.print(file.name());

            Serial.print("    ");

            Serial.print(file.size());

            Serial.print(" bytes    ");

            Serial.println(
                formatBytes(file.size())
            );

            file.close();
        }

        file = root.openNextFile();
    }

    root.close();
}

// ============================================================
// Print contents of a file
// Optional helper similar to: cat filename
// ============================================================

void printFile(const char *path)
{
    File file = SD.open(path);

    if (!file)
    {
        Serial.print("Cannot open file: ");
        Serial.println(path);
        return;
    }

    if (file.isDirectory())
    {
        Serial.print(path);
        Serial.println(" is a directory.");
        file.close();
        return;
    }

    Serial.println();
    Serial.println("========================================");
    Serial.print("FILE: ");
    Serial.println(path);
    Serial.println("========================================");

    while (file.available())
    {
        Serial.write(file.read());
    }

    Serial.println();
    Serial.println("========================================");

    file.close();
}

// ============================================================
// Setup
// ============================================================

void setup()
{
    Serial.begin(115200);

    delay(1000);

    Serial.println();
    Serial.println("========================================");
    Serial.println("ESP32 SD CARD TEST");
    Serial.println("========================================");

    Serial.println("Pin configuration:");
    Serial.println("CS   = GPIO5");
    Serial.println("SCK  = GPIO18");
    Serial.println("MISO = GPIO19");
    Serial.println("MOSI = GPIO15");

    Serial.println();
    Serial.println("Initializing SD card...");

    // Custom SPI pins
    SD_SPI.begin(
        SD_SCK,
        SD_MISO,
        SD_MOSI,
        SD_CS
    );

    // Same 20 MHz SD clock used in your logger code
    if (!SD.begin(
            SD_CS,
            SD_SPI,
            20000000))
    {
        Serial.println();
        Serial.println("SD INIT FAILED");

        Serial.println();
        Serial.println("Check:");
        Serial.println("1. SD card inserted");
        Serial.println("2. FAT32 formatting");
        Serial.println("3. CS   -> GPIO5");
        Serial.println("4. SCK  -> GPIO18");
        Serial.println("5. MISO -> GPIO19");
        Serial.println("6. MOSI -> GPIO15");
        Serial.println("7. GND is common");
        Serial.println("8. Correct SD module supply voltage");

        return;
    }

    Serial.println("SD INIT OK");

    // --------------------------------------------------------
    // Card information
    // --------------------------------------------------------

    printSDInfo();

    // --------------------------------------------------------
    // ls-style directory listing
    // --------------------------------------------------------

    Serial.println();
    Serial.println("========================================");
    Serial.println("SD CARD CONTENTS");
    Serial.println("========================================");

    listDirectory("/", 10);

    Serial.println();
    Serial.println("========================================");
    Serial.println("END OF DIRECTORY LIST");
    Serial.println("========================================");

    /*
      Optional:

      If you want to print a specific file like Linux:

          cat /sensor_data.csv

      uncomment this:

      printFile("/sensor_data.csv");
    */
}

// ============================================================
// Loop
// ============================================================

void loop()
{
}