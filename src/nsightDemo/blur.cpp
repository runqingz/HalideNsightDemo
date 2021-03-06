#include <stdio.h>
#include <string>
#include "Halide.h"
#include "clock.h"

using namespace std;
using namespace Halide;
using namespace Halide::Tools;

Target find_gpu_target();

class Blur {
public:
    Var n, x, y, c;
    Func producer, consumer;
    Buffer<float> input;
    Pipeline auto_blur;

    const string scheduler;

    // Constructor parameters:
    //  input: 4-D tensor of shape [batch_size, in_channels, in_height, in_width].
    Blur(Buffer<float> input, string scheduler)
        : input(input), scheduler(scheduler) {

        producer(n, c, x, y) = (input(n, c, x, y) + input(n, c, x+1, y) + input(n, c, x+2, y)) / 3; // Bluring vertically
        consumer(n, c, x, y) = (producer(n, c, x, y) + producer(n, c, x, y+1) + producer(n, c, x, y+2)) / 3; // Bluring horizontally

        auto_blur = Pipeline(consumer);
    }

    // Get a buffer with the shape of the output.
    Buffer<float> get_output_buffer() {
        Buffer<float> output(input.dim(0).extent(), input.dim(1).extent(), input.dim(2).extent()-2, input.dim(3).extent()-2);
        return output;
    }

    // Now a schedule that uses CUDA or OpenCL.
    bool schedule_for_gpu() {
        Var co, ci, xo, yo, xi, yi, tile_index, ni, no, t, ti, to, nc, nci, nco;

        Target target = find_gpu_target();
        if (!target.has_gpu_feature()) {
            return false;
        }

        if (scheduler.empty()) {
            if (target.has_feature(Target::CUDA)) {
                // n = 32
                // c = 8
                // x,y = 256
                //producer
                //    .compute_root()
                //    .fuse(n, c, nc)
                //    .tile(nc, x, nco, xo, nci, xi, 32, 32)
                //    .gpu_blocks(nco, y)
                //    .gpu_threads(nci, xi);

                consumer
                    .fuse(n, c, nc)
                    .tile(nc, x, nco, xo, nci, xi, 32, 32)
                    .gpu_blocks(nco, y)
                    .gpu_threads(nci, xi);

                producer
                    .compute_at(consumer, nci)
                    .store_in(MemoryType::Auto);
            }
            else {
                /*consumer.tile(x, y, x_outer, y_outer, x_inner, y_inner, 32, 32)
                    .fuse(x_outer, y_outer, tile_index)
                    .gpu_blocks(tile_index)
                    .gpu_threads(x_inner);*/
            }

            printf("Target: %s\n", target.to_string().c_str());
            consumer.compile_jit(target);
        }
        else {
            consumer.set_estimates({
               {0, input.dim(0).extent()},
               {0, input.dim(1).extent()},
               {0, input.dim(2).extent()},
               {0, input.dim(3).extent()} });

            auto_blur.auto_schedule(scheduler, target);
            auto_blur.compile_jit(target);
        }

        return true;
    }

    void test_performance(int num_runs = 100) {
        // Test the performance of the scheduled Blur.
        Buffer<float> output = this->get_output_buffer();

        // Run the filter once to initialize any GPU runtime state.
        if (scheduler.empty()) {
            consumer.realize(output);
        }
        else {
            auto_blur.realize(output);
        }

        // Run pipeline for multiple times.
        double total_time = 0.0;
        double best_time = 0.0;
        for (int i = 0; i < num_runs; i++) {

            double t1 = current_time();
            if (scheduler.empty()) {
                consumer.realize(output);
            }
            else {
                auto_blur.realize(output);
            }

            // Force any GPU code to finish by copying the buffer back to the CPU.
            output.device_sync();

            double t2 = current_time();

            double elapsed = (t2 - t1);
            if (i == 0 || elapsed < best_time) {
                best_time = elapsed;
            }
            total_time += elapsed;
        }
        printf("%d runs in total\n", num_runs);
        printf("Average: %1.4f milliseconds\n", total_time / num_runs);
        printf("Best: %1.4f milliseconds\n", best_time);
    }
};

int main(int argc, char** argv) {
    // Params:
    //   batch_size: number of images (in a single batch).
    //   channels_in: number of input channels (depth of the input).
    //   height: height of the image.
    //   width: width of the image.
    const int batch_size = 32, width = 258, height = 258, channels_in = 8;
    string scheduler = "";

    if (argc == 2) {
        printf("Running performance test for Blur with autoscheduler: %s.\n", argv[1]);
        scheduler = argv[1];
        load_plugin("autoschedule_li2018");
    }
    else if (argc == 1) {
        printf("Running performance test for Blur with manual schedule.\n");
    }
    else {
        fprintf(stderr, "Usage: .//relu_gpu [autoscheduler]\n");
        return 1;
    }

    // Generate random input.
    // Input shape follows TensorFlow convention (N, H, W, C)
    printf("Generating input with dimensions: batch_size: %d, height: %d, width: %d, channels: %d\n", batch_size, height, width, channels_in);

    Buffer<float> input(batch_size, channels_in, height, width);
    for (int b = 0; b < batch_size; b++) {
        for (int c = 0; c < channels_in; c++) {
            for (int h = 0; h < height; h++) {
                for (int w = 0; w < width; w++) {
                    input(b, c, h, w) = rand();
                }
            }
        }
    }

    printf("Running pipeline on GPU:\n");
    Blur blur_layer(input, scheduler);

    blur_layer.schedule_for_gpu();
    printf("Testing performance on GPU:\n");
    blur_layer.test_performance();

    return 0;
}

// A helper function to check if OpenCL, Metal or D3D12 is present on the host machine.

Target find_gpu_target() {
    // Start with a target suitable for the machine you're running this on.
    Target target = get_host_target();

    vector<Target::Feature> features_to_try;

    if (target.os == Target::OSX) {
        // OS X doesn't update its OpenCL drivers, so they tend to be broken.
        // CUDA would also be a fine choice on machines with NVidia GPUs.
        features_to_try.push_back(Target::Metal);
    }
    else {
        features_to_try.push_back(Target::CUDA);
    }
    // Uncomment the following lines to also try CUDA:
    // features_to_try.push_back(Target::CUDA);

    for (Target::Feature f : features_to_try) {
        Target new_target = target.with_feature(f);
        if (host_supports_target_device(new_target)) {
            return new_target;
        }
    }

    printf("Requested GPU(s) are not supported. (Do you have the proper hardware and/or driver installed?)\n");
    return target;
}
