# About #

Source code from the paper "Streaming G-Buffer Compression for Multi-Sample Anti-Aliasing" presented at High Performance Graphics in June, 2014.

From the paper's abstract: "We present a novel lossy compression algorithm for G-buffers that enables deferred shading applications with high visibility sampling rates. Our streaming compression method operates in a single geometry rendering pass with a fixed, but scalable, amount of per pixel memory. We demonstrate reduced memory requirements and improved performance, with minimal impact on image quality."

# Notes #
  1. This was tested on a Intel Iris Pro, Windows 7, and Visual Studio 2012. The driver version is 15.36.3.64.3907.
  1. There are two different implementations. The mode "Streaming SBAA Ndi" should be used for all comparisons.
  1. This supports the SDKMesh model format. See InitScene in main.cpp to load custom assets.