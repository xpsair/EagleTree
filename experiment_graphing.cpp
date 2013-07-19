#define BOOST_FILESYSTEM_VERSION 3
#include "ssd.h"
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <stdio.h>  /* defines FILENAME_MAX */
#include <unistd.h>   // chdir
#include <sys/stat.h> // mkdir
using namespace ssd;

static const bool GRAPH_TITLES = true;

double Experiment::CPU_time_user() {
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    struct timeval time = ru.ru_utime;

    // Calculate time in seconds
    double result = time.tv_sec + (time.tv_usec / M);
    return result;
}

double Experiment::CPU_time_system() {
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    struct timeval time = ru.ru_stime;

    // Calculate time in seconds
    double result = time.tv_sec + (time.tv_usec / M);
    return result;
}

double Experiment::wall_clock_time() {
    struct timeval time;
    gettimeofday(&time, NULL);

    // Calculate time in seconds
    double result = time.tv_sec + time.tv_usec / M;
    return result;
}

string Experiment::pretty_time(double time) {
	stringstream time_text;
	uint hours = (uint) floor(time / 3600.0);
	uint minutes = (uint) floor(fmod(time, 3600.0)/60.0);
	double seconds = fmod(time, 60.0);
	if (hours > 0) {
		time_text << hours;
		if (hours == 1) time_text << " hour, ";
		else time_text << " hours, ";
	}

	if (minutes > 0 || hours > 0) {
		time_text << minutes;
		if (minutes == 1) time_text << " minute and ";
		else time_text << " minutes and ";
	}

	time_text << seconds;
	if (seconds == 1) time_text << " second ";
	else time_text << " seconds ";

	time_text << "[" << time << "s]";

	return time_text.str();
}

// Plotting x number of experiments into one graph
void Experiment::graph(int sizeX, int sizeY, string title, string filename, int column, vector<Experiment_Result> experiments, int y_max, string subfolder) {
	// Write temporary file containing GLE script
    string scriptFilename =  filename + ".gle"; // Name of temporary script file
    ofstream gleScript;
    printf("%s\n", scriptFilename.c_str());
    gleScript.open(scriptFilename.c_str());

    string y_max_str;
    stringstream y_max_stream;
    y_max_stream << " max " << y_max;
    y_max_str = y_max == UNDEFINED ? "" : y_max_stream.str();

    gleScript <<
    "size " << sizeX << " " << sizeY << endl << // 12 8
    "set font texcmr" << endl <<
    "begin graph" << endl <<
    "   key pos tl offset -0.0 0 compact" << endl <<
    "   scale auto" << endl <<
    (GRAPH_TITLES ? "" : "!") << "   title  \"" << title << "\"" << endl <<
    "   xtitle \"" << experiments[0].variable_parameter_name << "\"" << endl <<
    "   ytitle \"" << experiments[0].column_names[column] << "\"" << endl <<
    "   yaxis min 0" << y_max_str << endl;

    for (uint i = 0; i < experiments.size(); i++) {
    	Experiment_Result e = experiments[i];
        gleScript <<
        "   data   \"" << e.data_folder << subfolder << (subfolder != "" ? "/" : "") << Experiment_Result::stats_filename << Experiment_Result::datafile_postfix << "\"" << " d"<<i+1<<"=c1,c" << column+1 << endl <<
        "   d"<<i+1<<" line marker " << markers[i] << " key " << "\"" << e.experiment_name << "\"" << endl;
    }

    gleScript << "end graph" << endl;
    gleScript.close();

    // Run gle to draw graph
    string gleCommand = "gle \"" + scriptFilename + "\" \"" + filename + "\"";
    cout << gleCommand << "\n";
    system(gleCommand.c_str());

    if (REMOVE_GLE_SCRIPTS_AGAIN) remove(scriptFilename.c_str()); // Delete temporary script file again
}

void Experiment::latency_plot(int sizeX, int sizeY, string title, string filename, int column, int variable_parameter_value, Experiment_Result experiment, int y_max) {
	chdir(experiment.data_folder.c_str());

	// Write temporary file containing GLE script
    string scriptFilename = experiment.graph_filename_prefix + filename + ".gle"; // Name of temporary script file
    std::ofstream gleScript;
    gleScript.open(scriptFilename.c_str());

    string y_max_str;
    stringstream y_max_stream;
    y_max_stream << " max " << y_max;
    y_max_str = y_max == UNDEFINED ? "" : y_max_stream.str();

    gleScript <<
    "size " << sizeX << " " << sizeY << endl << // 12 8
    "include \"graphutil.gle\"" << endl <<
    "set font texcmr" << endl <<
    "begin graph" << endl <<
    "   key pos tr offset -0.0 0 compact" << endl <<
    "   scale auto" << endl <<
    (GRAPH_TITLES ? "" : "!") << "   title  \"" << title << "\"" << endl <<
    "   xtitle \"" << "IO #" << "\"" << endl <<
    "   ytitle \"IO Latency (µs)\"" << endl <<
	"   data \"" << experiment.data_folder << Experiment_Result::latency_filename_prefix << variable_parameter_value << Experiment_Result::datafile_postfix << "\"" << endl <<
    "   xaxis min 0 max " << experiment.vp_num_IOs[variable_parameter_value][column-1] << endl << // nolast nofirst
    "   yaxis min 0" << y_max_str << endl <<
//    "   dticks off" << endl <<
//    "   yaxis min 0 max dmaxy(d" << column+5 << ")*1.05" << endl << // column+5 = max column
	"   d" << column << " marker dot msize 0.1" << endl <<
    "end graph" << endl;
    gleScript.close();

    // Run gle to draw graph
    string gleCommand = "gle \"" + scriptFilename + "\" \"" + filename + "\"";
    cout << gleCommand << "\n";
    system(gleCommand.c_str());

    if (REMOVE_GLE_SCRIPTS_AGAIN) remove(scriptFilename.c_str()); // Delete temporary script file again
}


void Experiment::waittime_boxplot(int sizeX, int sizeY, string title, string filename, int mean_column, Experiment_Result experiment) {

	chdir(experiment.data_folder.c_str());

	// Write temporary file containing GLE script
    string scriptFilename = experiment.data_folder.c_str() + filename + ".gle"; // Name of temporary script file
    std::ofstream gleScript;
    printf("%s\n", scriptFilename.c_str());
    gleScript.open(scriptFilename.c_str());

    gleScript <<
    "size " << sizeX << " " << sizeY << endl << // 12 8
    "include \"graphutil.gle\"" << endl <<
    "set font texcmr" << endl <<
    "begin graph" << endl <<
    "   key pos tl offset -0.0 0 compact" << endl <<
    "   scale auto" << endl <<
    (GRAPH_TITLES ? "" : "!") << "   title  \"" << title << "\"" << endl <<
    "   xtitle \"" << experiment.variable_parameter_name << "\"" << endl <<
    "   ytitle \"Wait time (µs)\"" << endl <<
	"   data \"" << experiment.data_folder << Experiment_Result::stats_filename << Experiment_Result::datafile_postfix << "\"" << endl <<
    "   xaxis min dminx(d1)-2.5 max dmaxx(d1)+2.5 dticks 5" << endl << // nolast nofirst
    "   dticks off" << endl <<
    "   yaxis min 0 max dmaxy(d" << mean_column+5 << ")*1.05" << endl << // mean_column+5 = max column
    "   draw boxplot bwidth 0.4 ds0 " << mean_column << endl;

    gleScript <<
    "end graph" << endl;
    gleScript.close();

    // Run gle to draw graph
    string gleCommand = "gle \"" + scriptFilename + "\" \"" + filename + "\"";
    cout << gleCommand << "\n";
    system(gleCommand.c_str());

    if (REMOVE_GLE_SCRIPTS_AGAIN) remove(scriptFilename.c_str()); // Delete temporary script file again
}

void Experiment::draw_graph(int sizeX, int sizeY, string outputFile, string dataFilename, string title, string xAxisTitle, string yAxisTitle, string xAxisConf, string command) {
    // Write temporary file containing GLE script
    string scriptFilename = outputFile + ".gle"; // Name of temporary script file
    std::ofstream gleScript;
    gleScript.open(scriptFilename.c_str());
    gleScript <<
    "size " << sizeX << " " << sizeY << endl << // 12 8
    "set font texcmr" << endl <<
    "begin graph" << endl <<
    "   " << "key pos tl offset -0.0 0 compact" << endl <<
    "   scale auto" << endl <<
//    "   nobox" << endl <<
    (GRAPH_TITLES ? "" : "!") << "   title  \"" << title << "\"" << endl <<
    "   xtitle \"" << xAxisTitle << "\"" << endl <<
    "   ytitle \"" << yAxisTitle << "\"" << endl <<
//    "   xticks off" << endl <<
    "   " << xAxisConf << endl <<
    "   yaxis min 0" << endl <<
    "   data   \"" << dataFilename << "\"" << endl <<
    command << endl <<
    "end graph" << endl;
    gleScript.close();

    // Run gle to draw graph
    string gleCommand = "gle \"" + scriptFilename + "\" \"" + outputFile + "\"";
    cout << gleCommand << "\n";
    system(gleCommand.c_str());

    if (REMOVE_GLE_SCRIPTS_AGAIN) remove(scriptFilename.c_str()); // Delete temporary script file again
}

void Experiment::waittime_histogram(int sizeX, int sizeY, string outputFile, Experiment_Result experiment, vector<int> points, int black_column, int red_column) {
	vector<string> commands;
	for (uint i = 0; i < points.size(); i++) {
		stringstream command;
		command << "hist 0 " << i << " \"" << Experiment_Result::waittime_filename_prefix << points[i] << Experiment_Result::datafile_postfix << "\" \"Wait time histogram (" << experiment.variable_parameter_name << " = " << points[i] << ")\" \"log min 1\" \"Event wait time (µs)\" " << max(experiment.max_waittimes[black_column], (red_column == -1 ? 0 : experiment.max_waittimes[red_column])) << " " << StatisticsGatherer::get_global_instance()->get_wait_time_histogram_bin_size() << " " << black_column << " " << red_column;
		commands.push_back(command.str());
	}

	multigraph(sizeX, sizeY, outputFile, commands);
}

void Experiment::cross_experiment_waittime_histogram(int sizeX, int sizeY, string outputFile, vector<Experiment_Result> experiments, double point, int black_column, int red_column) {
	vector<string> commands;
	double cross_experiment_max_waittime = 0;
	for (uint i = 0; i < experiments.size(); i++) {
		if (experiments[i].vp_max_waittimes.find(point) == experiments[i].vp_max_waittimes.end()) {
			printf("cross_experiment_waittime_histogram: experiment with variable parameter value %d not found. Skipping graph drawing.\n", point);
			return;
		}
		cross_experiment_max_waittime = max(cross_experiment_max_waittime, experiments[i].vp_max_waittimes[point][black_column]);
		if (red_column != -1) cross_experiment_max_waittime = max(cross_experiment_max_waittime, experiments[i].vp_max_waittimes[point][red_column]);
	}
	for (uint i = 0; i < experiments.size(); i++) {
		Experiment_Result& e = experiments[i];
		stringstream command;
		command << "hist 0 " << i << " \"" << e.data_folder << Experiment_Result::waittime_filename_prefix << point << Experiment_Result::datafile_postfix << "\" \"Wait time histogram (" << e.experiment_name << ", " << e.variable_parameter_name << " = " << point << ")\" \"log min 1\" \"Event wait time (µs)\" " << cross_experiment_max_waittime << " " << StatisticsGatherer::get_global_instance()->get_wait_time_histogram_bin_size() << " " << black_column << " " << red_column;
		printf("%s\n", command.str().c_str());
		commands.push_back(command.str());
	}

	multigraph(sizeX, sizeY, outputFile, commands);
}


void Experiment::age_histogram(int sizeX, int sizeY, string outputFile, Experiment_Result experiment, vector<int> points) {
	vector<string> settings;
	stringstream age_max;
	age_max << "age_max = " << experiment.max_age;
	settings.push_back(age_max.str());

	vector<string> commands;
	for (uint i = 0; i < points.size(); i++) {
		stringstream command;
		command << "hist 0 " << i << " \"" << Experiment_Result::age_filename_prefix << points[i] << Experiment_Result::datafile_postfix << "\" \"Block age histogram (" << experiment.variable_parameter_name << " = " << points[i] << ")\" \"on min 0 max " << experiment.max_age_freq << "\" \"Block age\" age_max " << SsdStatisticsExtractor::get_age_histogram_bin_size();
		commands.push_back(command.str());
	}

	multigraph(sizeX, sizeY, experiment.graph_filename_prefix + outputFile, commands, settings);
}

void Experiment::queue_length_history(int sizeX, int sizeY, string outputFile, Experiment_Result experiment, vector<int> points) {
	vector<string> commands;
	for (uint i = 0; i < points.size(); i++) {
		stringstream command;
		command << "plot 0 " << i << " \"" << Experiment_Result::queue_filename_prefix << points[i] << Experiment_Result::datafile_postfix << "\" \"Queue length history (" << experiment.variable_parameter_name << " = " << points[i] << ")\" \"on\" \"Timeline (µs progressed)\" \"Items in event queue\"";
		commands.push_back(command.str());
	}

	multigraph(sizeX, sizeY, experiment.graph_filename_prefix + outputFile, commands);
}

void Experiment::throughput_history(int sizeX, int sizeY, string outputFile, Experiment_Result experiment, vector<int> points) {
	vector<string> commands;
	for (uint i = 0; i < points.size(); i++) {
		stringstream command;
		command << "plot 0 " << i << " \"" << Experiment_Result::throughput_filename_prefix << points[i] << Experiment_Result::datafile_postfix << "\" \"Throughput history (" << experiment.variable_parameter_name << " = " << points[i] << ")\" \"on\" \"Timeline (µs progressed)\" \"Throughput (IOs/s)\" " << 2;
		commands.push_back(command.str());
	}

	multigraph(sizeX, sizeY, experiment.graph_filename_prefix + outputFile, commands);
}

// Draw multiple smaller graphs in one image
void Experiment::multigraph(int sizeX, int sizeY, string outputFile, vector<string> commands, vector<string> settings) {
	// Write temporary file containing GLE script
    string scriptFilename = outputFile + ".gle"; // Name of temporary script file
    std::ofstream gleScript;
    gleScript.open(scriptFilename.c_str());

	gleScript <<
	"std_sx = " << sizeX << endl <<
	"std_sy = " << sizeY << endl <<
	endl <<
	"hist_graphs = " << commands.size() << endl <<
	endl <<
	"pad = " << (commands.size() == 1 ? 2 : 2) << endl <<
	endl <<
	"size std_sx+pad std_sy*hist_graphs+pad" << endl <<
	"set font texcmr" << endl <<
	endl <<
	"sub hist xp yp data$ title$ yaxis$ xaxistitle$ xmax binsize black_column red_column" << endl <<
	"   default black_column 1" << endl <<
	"   default red_column -1" << endl <<
	"   amove xp*(std_sx/2)+pad yp*std_sy+pad" << endl <<
	"   begin graph" << endl <<
	"      fullsize" << endl <<
	"      size std_sx-pad std_sy-pad" << endl <<
	"      if red_column = -1 then" << endl <<
	"         key off" << endl <<
	"      end if" << endl <<
	"      data  data$" << endl <<
	(GRAPH_TITLES ? "" : "!") << "      title title$" << endl <<
	"      yaxis \\expr{yaxis$}" << endl <<
//	"      xaxis dticks int(xmax/25+1)" << endl <<
	"      xaxis min -binsize/2" << endl <<
	"      xsubticks off" << endl <<
	"      x2ticks off" << endl <<
	"      x2subticks off" << endl <<
	"      xticks length -.1" << endl <<
	"      if xmax>0 then" << endl <<
	"         xaxis max xmax+binsize/2" << endl <<
	"      end if" << endl <<
	"      xtitle xaxistitle$" << endl <<
	"      ytitle \"Frequency\"" << endl <<
	"      bar d\\expr{black_column} width binsize dist binsize fill gray" << endl <<
	"      if red_column <> -1 then" << endl <<
    "         bar d\\expr{red_column} width binsize/2 dist binsize fill red" << endl <<
    "      end if" << endl <<
	"   end graph" << endl <<
	"end sub" << endl <<
	endl <<
	"sub plot xp yp data$ title$ yaxis$ xaxistitle$ yaxistitle$ num_plots" << endl <<
	"   default num_plots 1" << endl <<
	"   amove xp*(std_sx/2)+pad yp*std_sy+pad" << endl <<
	"   begin graph" << endl <<
	"      fullsize" << endl <<
	"      size std_sx-pad std_sy-pad" << endl <<
	"      if num_plots <= 1 then" << endl <<
	"         key off" << endl <<
	"      end if" << endl <<
	"      data  data$" << endl <<
	(GRAPH_TITLES ? "" : "!") << "      title title$" << endl <<
	"      yaxis \\expr{yaxis$}" << endl <<
	"      xtitle xaxistitle$" << endl <<
	"      ytitle yaxistitle$" << endl <<
	"      d1 line" << endl <<
	"      if num_plots >= 2 then" << endl <<
	"         d2 line color red" << endl <<
	"      end if" << endl <<
	"   end graph" << endl <<
	"end sub" << endl;

	for (uint i = 0; i < settings.size(); i++) {
		gleScript << settings[i] << endl;
	}
	for (uint i = 0; i < commands.size(); i++) {
		gleScript << commands[i] << endl;
	}

	// Run gle to draw graph
	string gleCommand = "gle \"" + scriptFilename + "\" \"" + outputFile + "\"";
	cout << gleCommand << "\n";
	system(gleCommand.c_str());

    if (REMOVE_GLE_SCRIPTS_AGAIN) remove(scriptFilename.c_str()); // Delete temporary script file again
}

string Experiment::get_working_dir() {
	char cCurrentPath[FILENAME_MAX];
	if (!getcwd(cCurrentPath, sizeof(cCurrentPath))) { return "<?>"; }
	cCurrentPath[sizeof(cCurrentPath) - 1] = '\0'; /* not really required */
	string currentPath(cCurrentPath);
	return currentPath;
}

void Experiment::draw_graphs(vector<vector<Experiment_Result> > exps, string exp_folder) {
	int sx = 16;
	int sy = 8;
	//for (int i = 0; i < exps[0][0].column_names.size(); i++) {
		//printf("%d: %s\n", i, exps[0][0].column_names[i].c_str());
	//}
	printf("chdir %s\n", exp_folder.c_str());
	chdir(exp_folder.c_str());
	for (int i = 0; i < exps[0].size(); ++i) { // i = 0: GLOBAL, i = 1: EXPERIMENT, i = 2: WRITE_THREADS
		vector<Experiment_Result> exp;
		for (int j = 0; j < exps.size(); ++j) {
			exp.push_back(exps[j][i]);
		}
		cout << "chdir "<<exps[0][i].sub_folder.c_str() << "\n";
		int error = (exps[0][i].sub_folder != "") ? mkdir(exps[0][i].sub_folder.c_str(), 0755) : 0;
		//if (error != 0 && error != 17 /*EEXIST*/) printf("Error %d creating to folder %s\n", error, exps[0][i].data_folder.c_str());
		error = (exps[0][i].sub_folder != "") ? chdir(exps[0][i].sub_folder.c_str()) : 0;
		if (error != 0) printf("Error %d changing to folder %s\n", error, exps[0][i].sub_folder.c_str());

		Experiment::graph(sx, sy,   "Throughput", 				"throughput", 			26, exp, UNDEFINED);
		Experiment::graph(sx, sy,   "Write Throughput", 			"throughput_write", 	27, exp);
		Experiment::graph(sx, sy,   "Read Throughput", 			"throughput_read", 		28, exp);
		Experiment::graph(sx, sy,   "Num Erases", 				"num_erases", 			8, 	exp);
		Experiment::graph(sx, sy,   "Num Migrations", 			"num_migrations", 		3, 	exp);

		Experiment::graph(sx, sy,   "Write latency, mean", 		"Write latency, mean", 		9, 	exp);
		Experiment::graph(sx, sy,   "Write latency, max", 		"Write latency, max", 		14, exp);
		Experiment::graph(sx, sy,   "Write latency, std", 		"Write latency, std", 		15, exp);

		Experiment::graph(sx, sy,   "Read latency, mean", 		"Read latency, mean", 		16,	exp);
		Experiment::graph(sx, sy,   "Read latency, max", 		"Read latency, max", 		21, exp);
		Experiment::graph(sx, sy,   "Read latency, std", 		"Read latency, std", 		22, exp);

		Experiment::graph(sx, sy,   "Channel Utilization (%)", 	"Channel utilization", 		24, exp);
		Experiment::graph(sx, sy,   "GC Efficiency", 			"GC Efficiency", 			25, exp);

		Experiment_Result& e = exp[0];
		int num = e.vp_max_waittimes.size();

		for (auto& kv : e.vp_max_waittimes) {
		    string name = "waittime_histogram " + to_string(kv.first);
		    Experiment::cross_experiment_waittime_histogram(sx, sy/2, name, exp, kv.first, 1, 4);
		}

		/*Experiment_Runner::cross_experiment_waittime_histogram(sx, sy/2, "waittime_histogram 90", exp, 90, 1, 4);
		Experiment_Runner::cross_experiment_waittime_histogram(sx, sy/2, "waittime_histogram 80", exp, 80, 1, 4);
		Experiment_Runner::cross_experiment_waittime_histogram(sx, sy/2, "waittime_histogram 70", exp, 70, 1, 4);
		Experiment_Runner::cross_experiment_waittime_histogram(sx, sy/2, "waittime_histogram 60", exp, 60, 1, 4);*/
		/*if (i > 0)*/ { chdir(".."); }
	}
}

void Experiment::draw_experiment_spesific_graphs(vector<vector<Experiment_Result> > exps, string exp_folder, vector<int> x_vals) {
		int sx = 16;
		int sy = 8;

		uint mean_pos_in_datafile = std::find(exps[0][0].column_names.begin(), exps[0][0].column_names.end(), "Write latency, mean (µs)") - exps[0][0].column_names.begin();
		assert(mean_pos_in_datafile != exps[0][0].column_names.size());

		for (uint j = 0; j < exps.size(); j++) {
			vector<Experiment_Result>& exp = exps[j];
			for (uint i = 0; i < exp.size(); i++) {
				printf("%s\n", exp[i].data_folder.c_str());
				if (chdir(exp[i].data_folder.c_str()) != 0) printf("Error changing dir to %s\n", exp[i].data_folder.c_str());
				Experiment::waittime_boxplot  		(sx, sy,   "Write latency boxplot", "boxplot", mean_pos_in_datafile, exp[i]);
				Experiment::waittime_histogram		(sx, sy/2, "waittime-histograms-allIOs", exp[i], x_vals, 1, 4);
				Experiment::waittime_histogram		(sx, sy/2, "waittime-histograms-allIOs", exp[i], x_vals, true);
				Experiment::age_histogram			(sx, sy/2, "age-histograms", exp[i], x_vals);
				Experiment::queue_length_history		(sx, sy/2, "queue_length", exp[i], x_vals);
				Experiment::throughput_history		(sx, sy/2, "throughput_history", exp[i], x_vals);
			}
		}
}