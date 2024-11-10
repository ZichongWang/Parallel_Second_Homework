#include <iostream>
#include <gdal.h>
#include <gdal_priv.h>
#include <cpl_conv.h> // for CPLMalloc()
#include <mpi.h>
#include <vector>
#include <filesystem>
#include <chrono>

namespace fs = std::filesystem;

void calculate_band_mean(const char *filename, int rank, int size, int band_index) {
    GDALDatasetH dataset = GDALOpen(filename, GA_ReadOnly);
    if (dataset == nullptr) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return;
    }

    GDALRasterBandH band = GDALGetRasterBand(dataset, band_index + 1);
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

    // 使用 MPI_Reduce 将所有进程的均值收集到主进程中
    double global_band_mean = 0;
    MPI_Reduce(&band_mean, &global_band_mean, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        global_band_mean /= size; // 平均值除以进程数
        // std::cout << "Band " << band_index + 1 << " mean: " << global_band_mean << std::endl;
    }

    GDALClose(dataset);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <folder_path>" << std::endl;
        return 1;
    }

    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    GDALAllRegister();

    auto start_time = std::chrono::high_resolution_clock::now();

    fs::path folder_path = argv[1];
    if (!fs::exists(folder_path) || !fs::is_directory(folder_path)) {
        if (rank == 0) {
            std::cerr << "The provided path is not a valid directory: " << folder_path << std::endl;
        }
        MPI_Finalize();
        return 1;
    }

    int tif_file_count = 0;
    for (const auto &entry : fs::directory_iterator(folder_path)) {
        if (entry.path().extension() == ".tif") {
            GDALDatasetH dataset = GDALOpen(entry.path().string().c_str(), GA_ReadOnly);
            if (dataset == nullptr) {
                std::cerr << "Failed to open file: " << entry.path().string() << std::endl;
                continue;
            }

            int band_count = GDALGetRasterCount(dataset);
            GDALClose(dataset);

            std::vector<int> band_indices(band_count);
            for (int i = 0; i < band_count; ++i) {
                band_indices[i] = i;
            }

            // 使用 MPI_Scatter 将波段索引分散到各个进程
            int band_index;
            MPI_Scatter(band_indices.data(), 1, MPI_INT, &band_index, 1, MPI_INT, 0, MPI_COMM_WORLD);

            calculate_band_mean(entry.path().string().c_str(), rank, size, band_index);

            if (rank == 0) {
                tif_file_count++;
            }
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    if (rank == 0) {
        std::chrono::duration<double> elapsed_time = end_time - start_time;
        std::cout << "MPI Method: MPI_Scatter and MPI_Reduce" << std::endl;
        std::cout << "Number of CPU cores used: " << size << std::endl;
        std::cout << "Number of processed tif files: " << tif_file_count << std::endl;
        std::cout << "Total execution time: " << elapsed_time.count() << " seconds" << std::endl;
    }

    MPI_Finalize();
    return 0;
}