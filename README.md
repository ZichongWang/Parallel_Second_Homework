# 并行计算第二次作业
王子翀 2022201379

`band_mean_init.cpp`为老师给的原始文件。`Baseline.cpp`为去除MPI代码的纯串行。其他文件为使用对应MPI方法改进的并行代码。

以下是对原始代码的一些修改


## 文件夹读取 MPI_Gather
单个文件的处理速度极快，因此在原始代码中加入对文件夹的支撑。使用文件指针遍历文件夹内的`.tif`文件，进行计算。在输出中加入更多与并行有关的信息，包括method，Core，file number等。同时，由于文件较多，删去输出band均值的`cout`代码。

编译：
```
(myconda) root@w0D2aQ:~/Parallel/sentinel2# mpicxx -o MPI_Gather  MPI_Gather.cpp -lgdal
```
运行：
```
(myconda) root@w0D2aQ:~/Parallel/sentinel2# mpirun -np 4 ./MPI_Gather  /root/Parallel/sentinel2/dataset/chop_fire
MPI Method: MPI_Gather
Number of CPU cores used: 4
Number of processed tif files: 35
Total execution time: 0.565366 seconds
```

## Baseline: 串行
为了比较并行带来的提升，首先将所有MPI去除，得到串行的结果。

串行的编译较为简单：
```
(myconda) root@w0D2aQ:~/Parallel/sentinel2# g++ -o Baseline Baseline.cpp -lgdal
```
运行结果：
```
(myconda) root@w0D2aQ:~/Parallel/sentinel2# ./Baseline /root/Parallel/sentinel2/dataset/chop_fire
Serial Processing
Number of processed tif files: 35
Total execution time: 1.12507 seconds
```

## 使用MPI_Reduce
每个进程负责读取其指定的波段数据并计算局部和, 使用`MPI_Reduce`替代`MPI_Gather`将所有进程的数值收集到主进程中。

编译：
```
mpicxx -o MPI_Reduce  MPI_Reduce.cpp -lgdal
```

运行情况：
```
(myconda) root@w0D2aQ:~/Parallel/sentinel2# mpirun -np 4 ./MPI_Reduce  /root/Parallel/sentinel2/dataset/chop_fire
MPI Method: MPI_Reduce
Number of CPU cores used: 4
Number of processed tif files: 35
Total execution time: 0.562578 seconds
```

## 使用MPI_Sactter
这里使用`MPI_Scatter`将数据分散，然后使用`MPI_Reduce`将数据收集。大体逻辑和之前一致。

编译：
```
mpicxx -o MPI_Scatter  MPI_Scatter.cpp -lgdal
```
运行
```
(myconda) root@w0D2aQ:~/Parallel/sentinel2# mpirun -np 4 ./MPI_Scatter  /root/Parallel/sentinel2/dataset/chop_fire
MPI Method: MPI_Scatter and MPI_Reduce
Number of CPU cores used: 4
Number of processed tif files: 35
Total execution time: 0.196963 seconds
```

## 使用MPI_Bcast
这里每个进程计算分配给它的波段的平均值，使用`MPI_Reduce`将所有进程的波段平均值汇总到根进程，在根进程上，将每个波段的总和除以进程数，得到最终的平均值，最后使用`MPI_Bcast`将最终的波段平均值从根进程广播到所有其他进程。

编译：
```
mpirun -np 4 ./MPI_Bcast  /root/Parallel/sentinel2/dataset/chop_fire
```

运行：
```
(myconda) root@w0D2aQ:~/Parallel/sentinel2# mpirun -np 4 ./MPI_Bcast  /root/Parallel/sentinel2/dataset/chop_fire
MPI Method: MPI_Reduce and MPI_Bcast
Number of CPU cores used: 4
Number of processed tif files: 35
Total execution time: 0.567402 seconds
```

## 并行指标计算

并行主要有两个评价指标：加速比和效率。

**加速比**:
$$
S=\frac{T_1}{T_p}
$$
其中$T_1$是串行程序的执行时间，$T_p$是并行程序的执行时间，使用$p$个处理器。

**效率**
$$
E = \frac{S}{p} = \frac{T_1}{p \cdot T_p}
$$
$p$为处理器数量。


| 方法                       | 执行时间 (秒) | 加速比 \( S \) | 效率 \( E \) |
|----------------------------|---------------|----------------|--------------|
| 串行                       | 1.12507      |  --         |     --    |
| MPI_Gather                 | 0.565366     | 2.00           | 0.50         |
| MPI_Reduce                 | 0.562578     | 2.00           | 0.50         |
| MPI_Scatter and MPI_Reduce | 0.196963     | 5.71           | 1.43         |
| MPI_Reduce and MPI_Bcast   | 0.567402     | 1.98           | 0.495        |

## 指标分析

通过对不同方法的运行时间、加速比等的比较，可以分析得出：
1. `MPI_Scatter and MPI_Reduce`效果非常好，达到了最高的加速比和最佳效率，这表明并行化效果非常好，这可能是因为数据分散和收集的优化非常有效，减少了通信开销。
2. `MPI_Gather`和`MPI_Reduce`效果极为接近，二者在数据收集和汇总方面表现良好，但仍有改进空间。
3. `MPI_Reduce and MPI_Bcast`效果最差，但是也相比串行提高了性能。

因此`MPI_Scatter and MPI_Reduce`是最佳选择。

## 任务亮点
+ 进行串行和并行的效果对比。为了客观比较二者的性能差异，完全删除MPI相关的代码，得到纯粹的串行情况。
+ 探索多种并行方法。实现了多种并行方法，以及不同方法之间的混合使用。
+ 实现文件夹读取。读取多个文件，降低时间估计的方差。也将每次输出的时间从毫秒级提升到秒级，更可读。
# Parallel_Second_Homework
