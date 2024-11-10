#include <iostream>
#include <gdal.h>
#include <gdal_priv.h>
#include <cpl_conv.h> // for CPLMalloc()
#include <mpi.h>
#include <vector>

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

    if (rank == 0) {
        for (int i = 0; i < band_count; ++i) {
            std::cout << "Mean of band " << (i + 1) << ": " << all_band_means[i] << std::endl;
        }
    }

    GDALClose(dataset);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <GeoTIFF file>" << std::endl;
        return 1;
    }

    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    GDALAllRegister();
    calculate_band_mean(argv[1], rank, size);

    MPI_Finalize();
    return 0;
}
