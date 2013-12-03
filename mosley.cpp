#include <array>
#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <sstream>
#include <vector>
#include <cstdlib>
#include <opencv2/opencv.hpp>
#include <ueye.h>

// The general exception for errors related to camera operations.
struct Camera_exception : std::runtime_error {
    Camera_exception(const std::string& msg) : std::runtime_error{msg} {}
};

// The Camera class is an abstraction over the pair of uEye cameras.
// Initialization is explicit, but the object follows RAII semantics
// and will clean up any allocated memory on the cameras when the
// Camera object goes out of scope. Images are snapped in an alternating
// fashion between the two cameras.
class Camera {
public:
    static const int LEFT_DEV_ID = 1;
    static const int RIGHT_DEV_ID = 2;

    Camera() : cameras{{{LEFT_DEV_ID,nullptr,0}, {RIGHT_DEV_ID,nullptr,0}}} {}

    // Disallow copying and moving.
    Camera(const Camera&) = delete;
    Camera(const Camera&&) = delete;
    Camera& operator=(const Camera&) = delete;
    Camera& operator=(const Camera&&) = delete;

    ~Camera()
    {
        // Call the exit routine and free any memory for each camera.
        for (auto& camera : cameras)
            destroy(camera);
    }

    void initialize()
    {
        // There must be two available cameras to continue.
        int num_cams = 0;
        auto result = is_GetNumberOfCameras(&num_cams);
        if (num_cams < 2)
            throw Camera_exception{"two cameras not available"};

        // Initialize each camera and setup the auto exit handler.
        for (auto& camera : cameras)
            initialize(camera);
    }

    void snap()
    {
        using namespace std::chrono;
        time_point<system_clock> start, end;

        static size_t current = 0;
        static size_t count[2] = {0, 0};
        const Physical_camera& camera = cameras[current];

        // Use next camera when snap() is called again.
        current = (current+1) % cameras.size();

        start = system_clock::now();
        is_SetImageMem(camera.id, camera.mem, camera.mem_id);
        auto result = is_FreezeVideo(camera.id, IS_WAIT);
        if (result != IS_SUCCESS) {
            std::cerr << "is_FreezeVideo:" << result << '\n';
            return;
        }

#if 0
        const int sz = 3840*2748*24;
        char* dest = new char[sz];
        result = is_CopyImageMem(camera.id, camera.mem, camera.mem_id, dest);
        if (result != IS_SUCCESS)
            std::cerr << "CopyImage: " << result << '\n';
        std::vector<unsigned char> buf(dest, dest+sz);
        cv::Mat img = cv::imdecode(buf, 1);
        std::stringstream ss;
        ss << "images/camera-" << camera.id << "-" << count[current]++ << ".jpg";
        cv::imwrite(ss.str(), img);
        delete[] dest;
#endif

        IMAGE_FILE_PARAMS params;
        params.pnImageID = (UINT*)&camera.mem_id;
        params.ppcImageMem = &camera.mem;

        std::wstringstream ss;
        ss << "images/camera-" << camera.id << "-" << count[current]++ << ".jpg";
        params.pwchFileName = const_cast<wchar_t*>(ss.str().c_str());
        params.nFileType = IS_IMG_JPG;
        params.nQuality = 80;
        auto r = is_ImageFile(camera.id, IS_IMAGE_FILE_CMD_SAVE,
                (void*)&params, sizeof(params));
        switch (r) {
        case IS_SUCCESS:
            break;
        case IS_INVALID_PARAMETER:
            std::cerr << "IS_INVALID_PARAMETER\n";
            break;
        case IS_FILE_READ_INVALID_BMP_ID:
            std::cerr << "IS_FILE_READ_INVALID_BMP\n";
            break;
        case IS_FILE_READ_OPEN_ERROR:
            std::cerr << "IS_FILE_READ_OPEN_ERROR\n";
            break;
        case IS_NO_SUCCESS:
            std::cerr << "IS_NO_SUCCESS\n";
            break;
        case IS_NOT_SUPPORTED:
            std::cerr << "IS_NOT_SUPPORTED\n";
            break;
        default:
            std::cerr << "unknown:" << r << '\n';
            break;
        }

        end = system_clock::now();
        const auto elapsed = duration_cast<milliseconds>(end-start).count();
        std::cout << "camera:" << camera.id << " " << "time:" << elapsed << "ms\n";
    }

private:
    struct Physical_camera {
        HIDS id;
        mutable char* mem;
        mutable int mem_id;
    };

    std::array<Physical_camera, 2> cameras;

    void initialize(const Physical_camera& camera)
    {
        // Open the camera using the specified device id.
        HIDS handle = camera.id;
        auto result = is_InitCamera(&handle, nullptr);
        if (result != IS_SUCCESS)
            throw Camera_exception{"could not initialize camera"};

        // Enable automatic closing of the camera handle after a
        // camera has been removed on-the-fly; the entire memory
        // allocated by the driver is also released.
        result = is_EnableAutoExit(camera.id, IS_ENABLE_AUTO_EXIT);
        if (result != IS_SUCCESS)
            throw Camera_exception{"could not enable auto exit"};

        // Set the UI-1495LE-C cameras to operate in full 10MP mode
        // and allocate a memory buffer.
        const int width = 3840;
        const int height = 2748;
        const int bitspixel = 24;
        const int format = 21;
        is_AllocImageMem(camera.id, width, height, bitspixel,
                &camera.mem, &camera.mem_id);
        is_SetImageMem(camera.id, camera.mem, camera.mem_id);
        is_ImageFormat(camera.id, IMGFRMT_CMD_SET_FORMAT,
                const_cast<int*>(&format), sizeof(format));

        // Set the area of interest for this camera.
        IS_RECT aoi;
        aoi.s32X = 800;
        aoi.s32Y = 1372;
        aoi.s32Width = 3040;
        aoi.s32Height = 406;
        result = is_AOI(camera.id, IS_AOI_IMAGE_SET_AOI, &aoi, sizeof(aoi));
        if (result != IS_SUCCESS)
            throw Camera_exception{"could not set AOI for camera"};
    }

    void destroy(const Physical_camera& camera)
    {
        // Memory allocated with is_AllocImageMem will be released
        // after the call to is_ExitCamera. Do not handle errors thrown
        // by is_ExitCamera because there is nothing to recover.
        is_ExitCamera(camera.id);
    }
};

int main()
{
    try {
        Camera camera;
        camera.initialize();
        for (int i = 0; i < 10; i++)
            camera.snap();
    } catch (Camera_exception& e) {
        std::cerr << e.what() << '\n';
        exit(1);
    }
}
