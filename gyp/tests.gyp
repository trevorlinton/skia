# GYP file to build unit tests.
{
  'includes': [
    'apptype_console.gypi',
  ],
  'targets': [
    {
      'target_name': 'tests',
      'type': 'executable',
      'include_dirs' : [
        '../src/core',
        '../src/effects',
        '../src/image',
        '../src/lazy',
        '../src/images',
        '../src/pathops',
        '../src/pdf',
        '../src/pipe/utils',
        '../src/utils',
        '../tools/',

        # Needed for TDStackNesterTest.
        '../experimental/PdfViewer',
        '../experimental/PdfViewer/src',
      ],
      'includes': [
        'pathops_unittest.gypi',
      ],
      'sources': [
        '../tests/AAClipTest.cpp',
        '../tests/AndroidPaintTest.cpp',
        '../tests/AnnotationTest.cpp',
        '../tests/ARGBImageEncoderTest.cpp',
        '../tests/AtomicTest.cpp',
        '../tests/BitmapTest.cpp',
        '../tests/BitmapCopyTest.cpp',
        '../tests/BitmapGetColorTest.cpp',
        '../tests/BitmapHasherTest.cpp',
        '../tests/BitmapHeapTest.cpp',
        '../tests/BitSetTest.cpp',
        '../tests/BlitRowTest.cpp',
        '../tests/BlurTest.cpp',
        '../tests/CachedDecodingPixelRefTest.cpp',
        '../tests/CanvasTest.cpp',
        '../tests/CanvasStateTest.cpp',
        '../tests/ChecksumTest.cpp',
        '../tests/ClampRangeTest.cpp',
        '../tests/ClipCacheTest.cpp',
        '../tests/ClipCubicTest.cpp',
        '../tests/ClipStackTest.cpp',
        '../tests/ClipperTest.cpp',
        '../tests/ColorFilterTest.cpp',
        '../tests/ColorPrivTest.cpp',
        '../tests/ColorTest.cpp',
        '../tests/DataRefTest.cpp',
        '../tests/DeferredCanvasTest.cpp',
        '../tests/DequeTest.cpp',
        '../tests/DeviceLooperTest.cpp',
        '../tests/DiscardableMemoryPool.cpp',
        '../tests/DiscardableMemoryTest.cpp',
        '../tests/DocumentTest.cpp',
        '../tests/DrawBitmapRectTest.cpp',
        '../tests/DrawPathTest.cpp',
        '../tests/DrawTextTest.cpp',
        '../tests/DynamicHashTest.cpp',
        '../tests/EmptyPathTest.cpp',
        '../tests/ErrorTest.cpp',
        '../tests/FillPathTest.cpp',
        '../tests/FitsInTest.cpp',
        '../tests/FlatDataTest.cpp',
        '../tests/FlateTest.cpp',
        '../tests/FontHostStreamTest.cpp',
        '../tests/FontHostTest.cpp',
        '../tests/FontMgrTest.cpp',
        '../tests/FontNamesTest.cpp',
        '../tests/FrontBufferedStreamTest.cpp',
        '../tests/GeometryTest.cpp',
        '../tests/GifTest.cpp',
        '../tests/GLInterfaceValidation.cpp',
        '../tests/GLProgramsTest.cpp',
        '../tests/GpuBitmapCopyTest.cpp',
        '../tests/GpuColorFilterTest.cpp',
        '../tests/GpuDrawPathTest.cpp',
        '../tests/GrContextFactoryTest.cpp',
        '../tests/GrDrawTargetTest.cpp',
        '../tests/GradientTest.cpp',
        '../tests/GrMemoryPoolTest.cpp',
        '../tests/GrSurfaceTest.cpp',
        '../tests/GrUnitTests.cpp',
        '../tests/HashCacheTest.cpp',
        '../tests/ImageCacheTest.cpp',
        '../tests/ImageDecodingTest.cpp',
        '../tests/ImageFilterTest.cpp',
        '../tests/InfRectTest.cpp',
        '../tests/JpegTest.cpp',
        '../tests/LListTest.cpp',
        '../tests/LayerDrawLooperTest.cpp',
        '../tests/MD5Test.cpp',
        '../tests/MallocPixelRefTest.cpp',
        '../tests/MathTest.cpp',
        '../tests/MatrixTest.cpp',
        '../tests/Matrix44Test.cpp',
        '../tests/MemoryTest.cpp',
        '../tests/MemsetTest.cpp',
        '../tests/MessageBusTest.cpp',
        '../tests/MetaDataTest.cpp',
        '../tests/MipMapTest.cpp',
        '../tests/OnceTest.cpp',
        '../tests/OSPathTest.cpp',
        '../tests/PackBitsTest.cpp',
        '../tests/PaintTest.cpp',
        '../tests/ParsePathTest.cpp',
        '../tests/PathCoverageTest.cpp',
        '../tests/PathMeasureTest.cpp',
        '../tests/PathTest.cpp',
        '../tests/PathUtilsTest.cpp',
        '../tests/PDFPrimitivesTest.cpp',
        '../tests/PictureTest.cpp',
        '../tests/PictureUtilsTest.cpp',
        '../tests/PipeTest.cpp',
        '../tests/PixelRefTest.cpp',
        '../tests/PointTest.cpp',
        '../tests/PremulAlphaRoundTripTest.cpp',
        '../tests/QuickRejectTest.cpp',
        '../tests/RandomTest.cpp',
        '../tests/Reader32Test.cpp',
        '../tests/ReadPixelsTest.cpp',
        '../tests/ReadWriteAlphaTest.cpp',
        '../tests/RefCntTest.cpp',
        '../tests/RefDictTest.cpp',
        '../tests/RegionTest.cpp',
        '../tests/ResourceCacheTest.cpp',
        '../tests/RoundRectTest.cpp',
        '../tests/RuntimeConfigTest.cpp',
        '../tests/RTreeTest.cpp',
        '../tests/SHA1Test.cpp',
        '../tests/ScalarTest.cpp',
        '../tests/SerializationTest.cpp',
        '../tests/ShaderImageFilterTest.cpp',
        '../tests/ShaderOpacityTest.cpp',
        '../tests/skia_test.cpp',
        '../tests/SortTest.cpp',
        '../tests/SrcOverTest.cpp',
        '../tests/StreamTest.cpp',
        '../tests/StringTest.cpp',
        '../tests/StrokeTest.cpp',
        '../tests/SurfaceTest.cpp',
        '../tests/Test.cpp',
        '../tests/Test.h',
        '../tests/TestSize.cpp',
        '../tests/TileGridTest.cpp',
        '../tests/TLSTest.cpp',
        '../tests/TSetTest.cpp',
        '../tests/ToUnicode.cpp',
        '../tests/Typeface.cpp',
        '../tests/UnicodeTest.cpp',
        '../tests/UnitTestTest.cpp',
        '../tests/UtilsTest.cpp',
        '../tests/WArrayTest.cpp',
        '../tests/WritePixelsTest.cpp',
        '../tests/Writer32Test.cpp',
        '../tests/XfermodeTest.cpp',

        '../experimental/PdfViewer/src/SkTDStackNester.h',
        '../tests/TDStackNesterTest.cpp',

        # Needed for PipeTest.
        '../src/pipe/utils/SamplePipeControllers.cpp',
      ],
      'dependencies': [
        'skia_lib.gyp:skia_lib',
        'flags.gyp:flags',
        'experimental.gyp:experimental',
        'pdf.gyp:pdf',
        'tools.gyp:picture_utils',
      ],
      'conditions': [
        [ 'skia_gpu == 1', {
          'include_dirs': [
            '../src/gpu',
          ],
        }],
      ],
    },
  ],
}