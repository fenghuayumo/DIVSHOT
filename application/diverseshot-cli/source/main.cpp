#include <iostream>
#include <vector>
#include <filesystem>
#include <unordered_map>
#include "gs_train.hpp"

int main(int argc,const char *argv[])
{
	CLI::App app("gaussian_train");
	app.set_version_flag("--version", "1.0.0");
	app.allow_config_extras(CLI::config_extras_mode::error);
    uint32_t maxImageWidth  = 2048;
    uint32_t maxImageHeight = 2048;
	app.add_option("--maxImageWidth", maxImageWidth, "set max image width")->capture_default_str();
	app.add_option("--maxImageHeight", maxImageHeight, "set max image height")->capture_default_str();
	bool eval = false;
	app.add_flag("--eval", eval, "eval test set when trainning");
	std::string source_path, model_path = "../out_put/iteration";
	int max_iteration = 30000;
	int model_type = 0, densifyStrategy = 1;
	app.add_option("--inputPath", source_path, "set data set path");

	// add ssim loss
	float ssim_rate = 0.2;
	app.add_option("--ssim", ssim_rate, "set ssim loss rate, default is 0.2")->capture_default_str();

	app.add_option("--outputPath", model_path, "set trained model output path")->capture_default_str();
	app.add_option("--modelType", model_type, "set trained model type")->capture_default_str();
	app.add_option("--densifyStrategy", densifyStrategy,"set denisfy strategy")->capture_default_str();
	app.add_option("--maxIteration", max_iteration, "set max iterations")->capture_default_str();;
    //float pos_lr_init = 0.00016; float pos_lr_final = 0.0000016; float rot_lr = 0.001; float scaling_lr = 0.001; float feature_lr = 0.0025;float opacity_lr = 0.05;
    //app.add_option("--pos_lr_init",pos_lr_init,"set initial pos lr")->capture_default_str();
    //app.add_option("--pos_lr_final",pos_lr_final, "set final pos lr")->capture_default_str();
    //app.add_option("--rot_lr",rot_lr)->capture_default_str();
    //app.add_option("--scaling_lr", scaling_lr)->capture_default_str();
    //app.add_option("--feature_lr",feature_lr)->capture_default_str();
    //app.add_option("--opacity_lr",opacity_lr)->capture_default_str();
	bool progressive_train = true;
	app.add_option("--progressTrain", progressive_train,"whether progressive train")->capture_default_str();
	int load_itr = -1;
	app.add_option("--load_itr", load_itr,"loaditerations")->capture_default_str();
	bool pixel_grad_scale = false;
	//app.add_option("--pixgrad_scale", pixel_grad_scale)->capture_default_str();
	bool absgrad_densify = true;
	app.add_option("--absgrad", absgrad_densify)->capture_default_str();
	float growGrad2d = 0.0002;
	int warmupLength = 500;float refineEvery = 100;
	int resetAlphaEvery = 3000;int refineStopIter = 15000, refineScale2dStopIter= refineStopIter;
	float min_opacity = 0.005;
	bool  verbose = true, revisedOpacity = true, mipAntiliased = false,normalLoss = false;
	int pruneEvery = 70000;
	int pruneStrategy = 1, packLevel = 1;
	app.add_option("--growGrad2d",growGrad2d,"set densify grow grad threshold")->capture_default_str();
	app.add_option("--warmupLength",warmupLength,"start adding densification operation after warmupLength step ")->capture_default_str();
	app.add_option("--refineEvery",refineEvery,"how many every step do dulpicate and split operation")->capture_default_str();
	app.add_option("--resetAlphaEvery",resetAlphaEvery,"how many every step do alpha reset ")->capture_default_str();
	app.add_option("--refineStopIter",refineStopIter,"stop refine after this steps")->capture_default_str();
	app.add_option("--refineScale2dStopIter",refineScale2dStopIter)->capture_default_str();
	app.add_option("--minOpacity", min_opacity,"set min opacity prune threshold")->capture_default_str();
	//app.add_option("--verbose", verbose)->capture_default_str();
	app.add_option("--pruneEvery", pruneEvery,"how many every step do prune")->capture_default_str();
	app.add_option("--pruneStrategy", pruneStrategy,"set prune strategy, 0: reduce, 1: light")->capture_default_str();
	app.add_option("--revisedOpacity", revisedOpacity)->capture_default_str();
	app.add_option("--mipAntiliased", mipAntiliased,"whether enable mipAntiliased training")->capture_default_str();
	app.add_option("--packLevel", packLevel,"set pack level which can optimize memory usage.")->capture_default_str();
	app.add_option("--exportMesh", normalLoss,"whether enable normal loss")->capture_default_str();
	float noiselr = 1e5;
	app.add_option("--noiselr", noiselr,"set noiselr")->capture_default_str();
	bool useMask = false;
	app.add_option("--useMask", useMask,"set use mask")->capture_default_str();
	// Parse command line and update any requested settings
	try
	{
//	    app.parse(GetCommandLine(), true);
        app.parse(argc, argv);
	}
	catch (const CLI::ParseError &e)
	{
	    if (e.get_name() == "CallForHelp")
	    {
	        std::cout<< (app.help()) << std::endl;
	        return false;
	    }
	    else if (e.get_name() == "CallForAllHelp")
	    {
			std::cout << (app.help("", CLI::AppFormatMode::All)) << std::endl;
	        return false;
	    }
	    else if (e.get_name() == "CallForVersion")
	    {
			std::cout << ("{} ':v's {}", "gaussian_train", app.version()) << std::endl;
	        return false;
	    }
		std::cout << ("Command Line Error: {} ", e.what()) << std::endl;
	    return false;
	}
	
	train_gaussian(app);
	return 0;
}
