#define CATCH_CONFIG_MAIN
#include "catch.h"
#include <iostream>
#include "include/symd.h"
#include <chrono>
#include <algorithm>


namespace tests
{
    ////////////////////////////////////////////////////////////////////////////////////////////
    // Helpers
    ////////////////////////////////////////////////////////////////////////////////////////////

#ifdef _DEBUG
    constexpr int NUM_ITER = 1;
#else
    constexpr int NUM_ITER = 100;
#endif

    template <typename T>
    static void requireEqual(const T* data, const std::vector<T>& ref)
    {
        for (size_t i = 0; i < ref.size(); i++)
            REQUIRE(data[i] == ref[i]);
    }

    template <typename T>
    static void requireEqual(const std::vector<T>& data, const std::vector<T>& ref)
    {
        REQUIRE(data.size() == ref.size());

        requireEqual(data.data(), ref);
    }

    template <typename F>
    static auto executionTimeMs(F&& func)
    {
        auto t1 = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < NUM_ITER; i++)
        {
            func();
        }

        auto t2 = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double, std::milli> duration = t2 - t1;
        duration = duration / NUM_ITER;

        return duration;
    }


    ////////////////////////////////////////////////////////////////////////////////////////////
    // Tests
    ////////////////////////////////////////////////////////////////////////////////////////////

    TEST_CASE("Mapping 1")
    {
        std::vector<int> input = {1, 2, 3, 4, 5, 6, 7, 8, 9};
        std::vector<int> output(input.size());

        symd::map_single_core(output, [](auto x) { return x * 2; }, input);

        requireEqual(output, { 2, 4, 6, 8, 10, 12, 14, 16, 18 });
    }

    // Blend is substitute for "if" construct
    TEST_CASE("Mapping 2 - blend")
    {
        std::vector<int> input = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
        std::vector<int> output(input.size());

        symd::map_single_core(output, [](auto x)
            {
                return symd::kernel::blend(x != 3, x * 2, x);
            }, input);

        requireEqual(output, { 2, 4, 3, 8, 10, 12, 14, 16, 18 });
    }

    TEST_CASE("Mapping 2 - min")
    {
        std::vector<int> input = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18 };

        // Symd can work with spans
        std::span<int> span(&input[2], 11);
        std::vector<int> output(span.size());

        symd::map_single_core(output, [](auto x)
            {
                return std::min(x, 7);
            }, span);

        requireEqual(output, { 3, 4, 5, 6, 7, 7, 7, 7, 7, 7, 7 });
    }

    TEST_CASE("Mapping 2 - abs")
    {
        // Symd can work with std::array
        std::array<int, 9> input = { 1, -2, 3, -4, 5, -6, -7, 8, -9 };
        std::array<int, 9> output;

        symd::map_single_core(output, [](auto x)
            {
                return std::abs(2 * x);
            }, input);

        requireEqual(output.data(), { 2, 4, 6, 8, 10, 12, 14, 16, 18 });
    }

    TEST_CASE("Mapping 2 - convert")
    {
        std::vector<int> input = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
        std::vector<float> output(input.size());

        symd::map_single_core(output, [](auto x)
            {
                return symd::kernel::convert_to<float>(x * 2);
            }, input);

        requireEqual(output, { 2.f, 4.f, 6.f, 8.f, 10.f, 12.f, 14.f, 16.f, 18.f });
    }

    // Measure execution time
    TEST_CASE("Mapping 2 - exec time 1")
    {
        std::vector<float> input(2000000);
        std::vector<float> output(input.size());

        // initialize input
        //symd::map_single_core(input, [](){ return 1; });

        // Pass computation to measure time function. It fill execute it multiple times to measure time correctly.
        auto durationSymdSingleCore = executionTimeMs([&]()
            {
                symd::map_single_core(output, [](const auto& x)
                    {
                        return 2.f * x;
                    }, input);
            }
        );

        auto durationSymd = executionTimeMs([&]()
            {
                symd::map(output, [](const auto& x)
                    {
                        return 2.f * x;
                    }, input);
            }
        );

        // Measure time for simple for loop
        auto durationLoop = executionTimeMs([&]()
            {
                for (size_t i = 0; i < input.size(); i++)
                    output[i] = 2.f * input[i];
            }
        );

        std::cout << "Simple processing - Loop             : " << durationLoop.count() << " ms" << std::endl;
        std::cout << "Simple processing - symd_single_core : " << durationSymdSingleCore.count() << " ms" << std::endl;
        std::cout << "Simple processing - symd_multi_core  : " << durationSymd.count() << " ms" << std::endl << std::endl;
    }

    TEST_CASE("Mapping 2 - multi out")
    {
        std::vector<float> input = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
        std::vector<float> out1(input.size());
        std::vector<float> out2(input.size());

        auto outTuple = std::tie(out1, out2);

        symd::map_single_core(outTuple, [](auto x)
            {
                auto res1 = x * 2;
                auto res2 = x * 3;

                return std::array{res1, res2};
            }, input);

        requireEqual(out1, { 2.f, 4.f, 6.f, 8.f, 10.f, 12.f, 14.f, 16.f, 18.f });
        requireEqual(out2, { 3.f, 6.f, 9.f, 12.f, 15.f, 18.f, 21.f, 24.f, 27.f });
    }

    // Taking 0 inputs need to be added support for..
    TEST_CASE("Mapping - 0 inputs")
    {
        std::vector<int> output(11);

        //symd::map_single_core(output, []() { return 0; });
        //requireEqual(output, { 2, 4, 6, 8, 10, 12, 14, 16, 18 });
    }

    template <typename Y, typename U, typename V>
    auto yuvToRgbKernel(const Y& y, const U& u, const V& v)
    {
        auto yt = y - 16.f;
        auto ut = u - 128.f;
        auto vt = v - 128.f;

        auto r = yt * 1.164f + vt * 1.596f;
        auto g = yt * 1.164f - ut * 0.392f - vt * 0.813f;
        auto b = yt * 1.164f + ut * 2.017f;

        r = std::max(std::min(r, 255.f), 0.0f);
        g = std::max(std::min(g, 255.f), 0.0f);
        b = std::max(std::min(b, 255.f), 0.0f);

        return std::array{ r, g, b };
    }

    TEST_CASE("YUV444 planar to RGB planar")
    {
        std::vector<float> Y(1920 * 1080);
        std::vector<float> U(1920 * 1080);
        std::vector<float> V(1920 * 1080);

        std::vector<float> R(Y.size());
        std::vector<float> G(Y.size());
        std::vector<float> B(Y.size());

        auto outTuple = std::tie(R, G, B);

        auto duration = executionTimeMs([&]()
            {
                symd::map_single_core(outTuple, [](auto y, auto u, auto v)
                    {
                        return yuvToRgbKernel(y, u, v);
                    }, Y, U, V);
            }
        );

        auto durationLoop = executionTimeMs([&]()
            {
                for (int i = 0; i < Y.size(); i++)
                {
                    auto rgb = yuvToRgbKernel(Y[i], U[i], V[i]);

                    R[i] = rgb[0];
                    G[i] = rgb[1];
                    B[i] = rgb[2];
                }
            }
        );

        std::cout << "Mapping YUV444 planar to RGB planar - Loop             : " << durationLoop.count() << " ms" << std::endl;
        std::cout << "Mapping YUV444 planar to RGB planar - symd_single_core : " << duration.count() << " ms" << std::endl << std::endl;
    }

    TEST_CASE("Mapping - Basic Stencil")
    {
        std::vector<float> input = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18 };
        std::vector<float> output(input.size());

        symd::map_single_core(output, [](const auto& x)
            {
                return (x(0,-1) + x(0,0) + x(0,1)) / 3;

            }, symd::views::stencil(input, 3, 1));

        requireEqual(output, { 5.f / 3, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, (2*17.f + 18)/3 });
    }


    template <typename StencilView, typename DataType>
    auto conv3x3_Kernel(const StencilView& sv, const DataType* kernel)
    {
        return
            sv(-1, -1) * kernel[0] + sv(-1, 0) * kernel[1] + sv(-1, 1) * kernel[2] +
            sv(0, -1) * kernel[3] + sv(0, 0) * kernel[4] + sv(0, 1) * kernel[5] +
            sv(1, -1) * kernel[6] + sv(1, 0) * kernel[7] + sv(1, 1) * kernel[8];
    }

    TEST_CASE("Mapping - Convolucion 3x3")
    {
        size_t width = 1920;
        size_t height = 1920;

        std::vector<float> input(width * height);
        std::vector<float> output(input.size());

        // Kernel for convolution
        std::array<float, 9> kernel = { 1.f / 9 };

        // Prepare 2D view to data to do 2D convolution
        symd::views::data_view<float, 2> twoDInput(input.data(), width, height, width);
        symd::views::data_view<float, 2> twoDOutput(output.data(), width, height, width);

        /*auto duration = executionTimeMs([&]()
            {
                // Do the convolution. We also need 2D stencil view
                symd::map(twoDOutput, [&](const auto& x)
                    {
                        return conv3x3_Kernel(x, kernel.data());

                    }, symd::views::stencil(twoDInput, 3, 3));
            }
        );

        auto durationSingleCore = executionTimeMs([&]()
            {
                // Do the convolution. We also need 2D stencil view
                symd::map_single_core(twoDOutput, [&](const auto& x)
                    {
                        return conv3x3_Kernel(x, kernel.data());

                    }, symd::views::stencil(twoDInput, 3, 3));
            }
        );

        auto durationLoop = executionTimeMs([&]()
            {
                for (int i = 1; i < height-1; i++)
                {
                    for (int j = 1; j < width-1; j++)
                    {
                        float res =
                            twoDInput.readPix(i - 1, j - 1) * kernel[0] +
                            twoDInput.readPix(i - 1, j) * kernel[1] +
                            twoDInput.readPix(i - 1, j + 1) * kernel[2] +

                            twoDInput.readPix(i, j - 1) * kernel[3] +
                            twoDInput.readPix(i, j) * kernel[4] +
                            twoDInput.readPix(i, j + 1) * kernel[5] +

                            twoDInput.readPix(i + 1, j - 1) * kernel[6] +
                            twoDInput.readPix(i + 1, j) * kernel[7] +
                            twoDInput.readPix(i + 1, j + 1) * kernel[8];

                        output[i * width + j] = res;
                    }
                }
            }
        );

        std::cout << "Convolution 3x3 - Loop             : " << durationLoop.count() << " ms" << std::endl;
        std::cout << "Convolution 3x3 - symd_single_core : " << durationSingleCore.count() << " ms" << std::endl;
        std::cout << "Convolution 3x3 - symd_multi_core  : " << duration.count() << " ms" << std::endl << std::endl;*/
    }

}

