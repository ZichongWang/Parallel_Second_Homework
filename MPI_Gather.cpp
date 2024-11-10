#include <iostream>
#include <gdal.h>
#include <gdal_priv.h>
#include <cpl_conv.h> // for CPLMalloc()
#include <mpi.h>
#include <vector>
#include <filesystem>
#include <chrono>

namespace fs = std::filesystem;

void calculate_band_mean(const char *filename, int rank, int size) {
    GDALDatasetH dataset = GDALOpen(filename, GA_ReadOnly);
    if (dataset == nullptr) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return;
    }

    int band_count = GDALGetRasterCount(dataset);
    int bands_per_process = band_count / size;
    int start_band = rank * bands_per_process;
    int end_band = (rank == size - 1) ? band_count : start_band + bands_per_process;

    std::vector<double> band_means(end_band - start_band);

    for (int i = start_band; i < end_band; ++i) {
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

        band_means[i - start_band] = (count > 0) ? (sum / count) : 0;
    }

    // 收集所有进程的均值
    std::vector<double> all_band_means;
    if (rank == 0) {
        all_band_means.resize(band_count);
    }

    MPI_Gather(band_means.data(), end_band - start_band, MPI_DOUBLE, 
               all_band_means.data(), end_band - start_band, MPI_DOUBLE, 
               0, MPI_COMM_WORLD);

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
            calculate_band_mean(entry.path().string().c_str(), rank, size);
            if (rank == 0) {
                tif_file_count++;
            }
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    if (rank == 0) {
        std::chrono::duration<double> elapsed_time = end_time - start_time;
        std::cout << "MPI Method: MPI_Gather" << std::endl;
        std::cout << "Number of CPU cores used: " << size << std::endl;
        std::cout << "Number of processed tif files: " << tif_file_count << std::endl;
        std::cout << "Total execution time: " << elapsed_time.count() << " seconds" << std::endl;
    }

    MPI_Finalize();
    return 0;
}
