#include <iostream>
#include <gdal.h>
#include <gdal_priv.h>
#include <cpl_conv.h> // for CPLMalloc()
#include <vector>
#include <filesystem>
#include <chrono>

namespace fs = std::filesystem;

void calculate_band_mean(const char *filename) {
    GDALDatasetH dataset = GDALOpen(filename, GA_ReadOnly);
    if (dataset == nullptr) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return;
    }

    int band_count = GDALGetRasterCount(dataset);

    for (int i = 0; i < band_count; ++i) {
        GDALRasterBandH band = GDALGetRasterBand(dataset, i + 1);
        int x_size = GDALGetRasterBandXSize(band);
        int y_size = GDALGetRasterBandYSize(band);

        std::vector<float> data(x_size * y_size);
        GDALRasterIO(band, GF_Read, 0, 0, x_size, y_size, data.data(), x_size, y_size, GDT_Float32, 0, 0);

        double sum = 0;
        int count = 0;

        for (float value : data) {
            if (value != 0) { // 假设0是无效值
                sum += value;
                count++;
            }
        }

        double band_mean = (count > 0) ? (sum / count) : 0;
        // std::cout << "Band " << i + 1 << " mean: " << band_mean << std::endl;
    }

    GDALClose(dataset);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <folder_path>" << std::endl;
        return 1;
    }

    GDALAllRegister();

    auto start_time = std::chrono::high_resolution_clock::now();

    fs::path folder_path = argv[1];
    if (!fs::exists(folder_path) || !fs::is_directory(folder_path)) {
        std::cerr << "The provided path is not a valid directory: " << folder_path << std::endl;
        return 1;
    }

    int tif_file_count = 0;
    for (const auto &entry : fs::directory_iterator(folder_path)) {
        if (entry.path().extension() == ".tif") {
            calculate_band_mean(entry.path().string().c_str());
            tif_file_count++;
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed_time = end_time - start_time;
    std::cout << "Serial Processing"<< std::endl;
    std::cout << "Number of processed tif files: " << tif_file_count << std::endl;
    std::cout << "Total execution time: " << elapsed_time.count() << " seconds" << std::endl;

    return 0;
}