#include <vector>
#include <string>

#if defined(_WIN32) || defined(__CYGWIN__)
#define OPEN3D_DLL_IMPORT __declspec(dllimport)
#define OPEN3D_DLL_EXPORT __declspec(dllexport)
#else
#define OPEN3D_DLL_IMPORT [[gnu::visibility("default")]]
#define OPEN3D_DLL_EXPORT [[gnu::visibility("default")]]
#endif

#if defined(O3D_ENABLE_DLL_EXPORTS)
#define O3D_API OPEN3D_DLL_EXPORT
#else
#define O3D_API OPEN3D_DLL_IMPORT
#endif

namespace o3d
{ 
	struct CameraIntrincs
	{
		float fx;
		float fy;
		float cx;
		float cy;
		int width;
		int height;
	};

    class O3D_API TSDF
	{
	public:
		void export_mesh(
			const std::string& filePath,
			const std::vector<double*>& viewMatrixs,
			const std::vector< CameraIntrincs> intrinsic,
			const std::vector<uint8_t*> rgb,
			const std::vector<float*>& depth,
			float voxel_length = 1/256.0f,
            int target_triangles = 200000,
            int target_tex_resolution = 512,
			float sdf_trunc = 0.05f,
            float depth_trunc = 1.0f,
			int threshold_ntri = 100000
			);

		//std::vector<float> filter_outlier_points(
  //                      const std::vector<float>& points, int nb_points = 10,
		//				float radius = 0.1f);
                float get_progress() { return progress_ ;}
	protected:
		float progress_ = 0.0f;
	};

	struct Vec3 {
		float x,y,z;
	};
        O3D_API std::array<Vec3, 8> generate_obj_bound(
            const std::vector<double>& rgbs,
            const std::vector<double>& xyzs,
			bool aabb = false,
            size_t nb_radius = 10,
            float radius = 0.1
		);
	//rgbs is colors array which size is NX3, xyzs is NX3
        O3D_API std::tuple<uint8_t*, float*, size_t> uniform_down_sample_points(
			const std::vector<uint8_t>& rgbs,
            const std::vector<float>& xyzs,
            size_t every_k = 16);
    // rgbs is colors array which size is NX3, xyzs is NX3
    O3D_API void uniform_down_sample_points_to_file(
                const std::string& filePath,
                const std::vector<uint8_t>& rgsb,
                const std::vector<float>& xyzs,
                size_t every_k = 16,
				bool to_blender_coord = true);
    O3D_API std::tuple<uint8_t*, float*, size_t> voxel_down_sample_points(
            const std::vector<uint8_t>& rgbs,
            const std::vector<float>& xyzs,
            double voxel_size = 0.05);
    // rgbs is colors array which size is NX3, xyzs is NX3
    O3D_API void voxel_down_sample_points_to_file(
            const std::string& filePath,
            const std::vector<uint8_t>& rgsb,
            const std::vector<float>& xyzs,
            double voxel_size = 0.05,
            bool to_blender_coord = true);
    // points_rgb is colors array which size is NX3, points_xyz is NX3
	O3D_API void write_point_cloud(const std::string& filePath,
                                   const std::vector<float>& points_xyz,
                                const std::vector<uint8_t>& points_rgb);
	O3D_API void write_triangle_mesh(
		 const std::string& filePath,
		const std::vector<double>&	vertices,
		const std::vector<uint32_t>&	indices
	);

	O3D_API double compute_avg_spacing(const double* points,
									int num_pts,
                                    int nb_points = 10,
                                    float radius = 0.1,
                                    int k = 5);
        }