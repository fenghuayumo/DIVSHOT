#include <indicators/indicators.hpp>
#include "gs_train.hpp"
#include <gaussian_trainer_scene.hpp>
#include <core/plugin.h>
#include <utility/file_utils.h>
#include <format>
void train_gaussian(const CLI::App& app)
{
    //gstrain_init();
    diverse::debug::Log::init();
    typedef void(*voidFunc)();
    auto dll_path = diverse::parentDirectory(diverse::getExecutablePath());
    diverse::add_dll_directory(dll_path + "/../Python/Lib/site-packages/torch/lib");
    diverse::add_dll_directory(dll_path + "/sfm",false);
#ifdef DS_PLATFORM_MACOS
    auto plugin_path = "gstrain";//diverse::parentDirectory(dll_path) + "/Frameworks/gstrain";
#else
    auto plugin_path = "gstrain";
#endif
    if (!diverse::PluginManager::get().ensure_plugin_loaded(plugin_path)) {
        exit(-1);
    }
    auto plugin = diverse::PluginManager::get().get_plugin(plugin_path);
    auto gsplat_init = (voidFunc)plugin->get_symbol("gstrain_init");
    if(!gsplat_init) 
    {
        std::cerr << "plugin init failed " << std::endl;
        exit(-1);
    }
    gsplat_init();
    auto source_path = app.get_option("--inputPath")->as<std::string>();
    //auto pos_lr_init = app.get_option("--pos_lr_init")->as<float>();
    //auto pos_lr_final = app.get_option("--pos_lr_final")->as<float>();
    //auto rot_lr = app.get_option("--rot_lr")->as<float>();
    //auto scaling_lr = app.get_option("--scaling_lr")->as<float>();
    //auto feature_lr = app.get_option("--feature_lr")->as<float>();
    //auto opacity_lr = app.get_option("--opacity_lr")->as<float>();
    const int refineStopIter = app.get_option("--refineStopIter")->as<int>();
    const int warmupLength = app.get_option("--warmupLength")->as<int>();
    const int  refineEvery = app.get_option("--refineEvery")->as<int>();
    const int resetAlphaEvery = app.get_option("--resetAlphaEvery")->as<int>();
    const float growGrad2d =  app.get_option("--growGrad2d")->as<float>();
    const int max_iteraion = app.get_option("--maxIteration")->as<int>();
    auto min_opacity = app.get_option("--minOpacity")->as<float>();
    const auto progress_train = app.get_option("--progressTrain")->as<bool>();
    auto ssim_rate = app.get_option("--ssim")->as<float>();
    auto load_itr = app.get_option("--load_itr")->as<int>();
    //calculate time consume
    auto start = std::chrono::high_resolution_clock::now();
    GaussianTrainConfig train_config;
    train_config.sourcePath = source_path;
    //train_config.poslrInit = pos_lr_init;
    //train_config.poslrFinal = pos_lr_final;
    //train_config.rotationlr = rot_lr;
    //train_config.scalinglr = scaling_lr;
    //train_config.featurelr = feature_lr;
    //train_config.opacitylr = opacity_lr;
    train_config.growGrad2d = growGrad2d;
    train_config.warmupLength = warmupLength;
    train_config.refineEvery = refineEvery;
    train_config.resetAlphaEvery = resetAlphaEvery;
    train_config.refineStopIter = refineStopIter;
    train_config.revisedOpacity =  app.get_option("--revisedOpacity")->as<bool>();
    train_config.refineScale2dStopIter = app.get_option("--refineScale2dStopIter")->as<int>();
    //train_config.min_opacity = min_opacity;
    train_config.ssimWeight = ssim_rate;
    train_config.modelPath = app.get_option("--outputPath")->as<std::string>();
    train_config.modelType = app.get_option("--modelType")->as<int>();
    train_config.densifyStrategy = app.get_option("--densifyStrategy")->as<int>();
    train_config.progressiveTrain = progress_train;
    //train_config.pixelGradScale = app.get_option("--pixgrad_scale")->as<bool>();
    train_config.useAbsGrad = app.get_option("--absgrad")->as<bool>();
    //train_config.verbose = app.get_option("--verbose")->as<bool>(); 
    train_config.pruneInterval = app.get_option("--pruneEvery")->as<int>();
    train_config.pruneStrategy = app.get_option("--pruneStrategy")->as<int>();
    train_config.mipAntiliased = app.get_option("--mipAntiliased")->as<bool>();
    train_config.maxImageHeight = app.get_option("--maxImageHeight")->as<int>();
    train_config.maxImageWidth = app.get_option("--maxImageWidth")->as<int>();
    train_config.exportMesh = app.get_option("--exportMesh")->as<bool>();
    train_config.numIters = max_iteraion;
    if(train_config.exportMesh) {
        train_config.normalConsistencyLoss = true;
        train_config.useMask = true;
    }
    // train_config.visibleAdam = true;
    train_config.verbose = true;
    auto packLevel = app.get_option("--packLevel")->as<int>();
    if(packLevel == 0)
        train_config.packLevel = 0;
    else if(packLevel == 1)
        train_config.packLevel = GSPackLevel::PackF32ToU8;
    else
        train_config.packLevel = GSPackLevel::PackF32ToU8 | GSPackLevel::PackTileID;
    // train_config.packLevel = 3;
    // train_config.progressiveTrain = false;
    //train_config.pruneOpacity = 0.1;
    //train_config.pruneScale3d = 0.5;
    //train_config.pruneScale2d = 0.15;
    //auto scene = std::make_shared<GaussianTrainerScene>(train_config,-1);
    typedef void*(*createFunc)(const GaussianTrainConfig& config, int loadItr);
    auto create_splat = (createFunc)plugin->get_symbol("create_splat");
    auto scene = (GaussianTrainerScene*)create_splat(train_config, load_itr);
    typedef bool(*loadDataFunc)(GaussianTrainerScene* scene,const std::string& path);
    auto loadData = (loadDataFunc)plugin->get_symbol("load_train_data");
    if (loadData) {
        auto ret = loadData(scene,source_path);
        if (!ret)
        {
            std::cout << "load data failed!, please check source_path \n";
            exit(-1);
        }
    }
    else 
    {
        std::cout << "can't find load_train_data symbol\n";
        exit(-1);
    }
    // scene->pruenIteraions.push_back(10000);
    auto end = std::chrono::high_resolution_clock::now();
    //std::cout << "Preprocess time cost: " << std::chrono::duration_cast<std::chrono::seconds>(end - start).count() << " seconds" << std::endl;
    using namespace indicators;
    show_console_cursor(false);

    indicators::ProgressBar bar{
      option::BarWidth{50},
      option::Start{" ["},
      option::Fill{"█"},
      option::Lead{"█"},
      option::Remainder{"-"},
      option::End{"]"},
      option::PrefixText{"Training"},
      option::ForegroundColor{Color::green},
      option::ShowElapsedTime{true},
      option::ShowRemainingTime{true},
      option::FontStyles{std::vector<FontStyle>{FontStyle::bold}}
    };
    bar.set_option(option::PostfixText{ "Preprocess done" });
    bar.set_progress(1);
    typedef void(*SceneParamFunc)(GaussianTrainerScene* scene);
    auto train_step = (SceneParamFunc)plugin->get_symbol("train_step");
    auto save_splat_model = (SceneParamFunc)plugin->get_symbol("save_splat_model");
    auto export_mesh = (SceneParamFunc)plugin->get_symbol("export_mesh");
    auto delete_splat = (SceneParamFunc)plugin->get_symbol("delete_splat");
	for(int i = load_itr; i < max_iteraion; i++)
    {
        train_step(scene);
        if (i % 500 == 0)
        {
            end = std::chrono::high_resolution_clock::now();
            //std::cout << std::format("iteration : {} cost time : {} seconds \n", i, std::chrono::duration_cast<std::chrono::seconds>(end - start).count());
            bar.set_option(option::PostfixText{ std::format("train step : {}/{}",i, max_iteraion) });
            const int progress = (i / (float)max_iteraion) * 100;
            bar.set_progress(progress);
        }
        if(i > train_config.resetAlphaEvery && i % 10000 == 0)
            save_splat_model(scene);
	}
    bar.set_option(option::PostfixText{"Train Done"});
    bar.set_progress(100);
    if (train_config.exportMesh)
        export_mesh(scene);
    // Show cursor
    show_console_cursor(true);
    end = std::chrono::high_resolution_clock::now();
    std::cout << std::format("train cost time : {} seconds\n", std::chrono::duration_cast<std::chrono::seconds>(end - start).count());
    save_splat_model(scene);
    delete_splat(scene);
    auto gstrain_destroy = (voidFunc)plugin->get_symbol("gstrain_destroy");
    gstrain_destroy();
    diverse::debug::Log::release();
}
