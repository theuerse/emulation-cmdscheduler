#include <iostream>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <sstream>

#define PIPE_READ 0
#define PIPE_WRITE 1

#define WRITE_LOGFILE

using namespace std;

typedef std::vector<std::pair<double,std::string>>  IntervalVector;

boost::asio::io_service io;
boost::asio::deadline_timer timer(io);
IntervalVector intervals;
std::string priority_command("sudo chrt -r -p 50 ");

void readFileContents(std::string path){
	std::ifstream infile(path);

	// relative-time-offset	command
	// 1.001667		printf "1"
	std::string line;
	double time_offset;
	std::string cmds;
	
	while(std::getline(infile, line)){
		std::istringstream iss(line);

		if(!(iss >> time_offset)){
			std::cout << "invalid time-offset at the current line (" << intervals.size() << ") -> aborting ..." << std::endl;
			infile.close();
			exit(EXIT_FAILURE);
		}
		cmds = line.substr(line.find_first_of(" \t")+1); //TODO: search for better way?

		intervals.push_back(std::make_pair(time_offset, cmds));
	}
	infile.close();	
}

void scheduler(boost::posix_time::ptime start, int stdInPipe[]){
	int len;
	boost::posix_time::ptime target;
	long time_offset;
	std::string cmds;

	#ifdef WRITE_LOGFILE
    stringstream logBuffer;
    logBuffer << "start: " << start << '\n';
	#endif

	for (IntervalVector::iterator it = intervals.begin(); it != intervals.end(); ++it){
		time_offset = it->first * 1000;  // seconds to ms
		cmds = it->second;

		target = start + boost::posix_time::milliseconds(time_offset);

		timer.expires_at(target);
		timer.wait();

		#ifdef WRITE_LOGFILE
        logBuffer << boost::posix_time::microsec_clock::universal_time() << '\t' << cmds << "\n";
		#endif

		// the time has come, pipe command to child-shell
		cmds = cmds + "\n";       // add newline to end cmd-entry in "remote"-shell
		len = cmds.length() + 1; // account for ZERO terminating cstring
		if(write(stdInPipe[PIPE_WRITE], cmds.c_str(), len) != len){
			std::cerr << "short write to child/shell" << std::endl;
			exit(EXIT_FAILURE);
		}
		//#ifdef WRITE_LOGFILE
		//outputFile << boost::posix_time::microsec_clock::universal_time() << "\tend\t" << cmds << "\n";
		//#endif
	}
	#ifdef WRITE_LOGFILE
    logBuffer << "end: " << boost::posix_time::microsec_clock::universal_time() << '\n';
    std::ofstream outputFile("./cmdScheduler.log");
    outputFile << logBuffer.str();
	outputFile.close();
	#endif
}

int main(int argc, char* argv[])
{
	if(argc != 2 && argc != 4){
		std::cerr << "Usage: " << argv[0] << " cmd-file.cmd" << std::endl;
		std::cerr << "Usage: " << argv[0] << " cmd-file.cmd  2017-07-14 09:22:00.000" << std::endl;
		std::cerr << "\twhere 2017-07-14 09:22:00.000 is UTC and STRICTLY IN THE FUTURE!" << std::endl;
		exit(EXIT_FAILURE);
	}

	// prepare everything necessary beforehand
	readFileContents(argv[1]);

	boost::posix_time::ptime start;
	if(argc == 4){
		start = boost::posix_time::time_from_string(std::string(argv[2]) +  " " + std::string(argv[3])); // "2005-12-07 23:59:59.000"
	}else {
		start = boost::posix_time::microsec_clock::universal_time();
	}
	std::cout << "start: " << start << std::endl;

	int stdInPipe[2];
	if(pipe(stdInPipe) < 0){
		std::cerr << "Allocation of pipe for communication with child failed" << std::endl;
		exit(EXIT_FAILURE);
	}

	// re-set own process priority (and subsequently that of the child-processes)
	std::ostringstream oss;
	oss << priority_command << getpid();
	if(system(oss.str().c_str()) == -1){
		std::cerr << "priority-change using 'chrt' failed, aborting ..." << std::endl;
		exit(EXIT_FAILURE);
	}

	pid_t pid = fork();

	if(pid == -1){
		//
		// FAIL
		//
		std::cerr << "Fork failed :-(" << std::endl;

		// teardown pipe
		close(stdInPipe[PIPE_READ]);
		close(stdInPipe[PIPE_WRITE]);

		return 1;
	} else if (pid > 0){
		//
		// PARENT
		//
		// close unneccesary file-descriptors
		close(stdInPipe[PIPE_READ]);

		// wait until start-time
		timer.expires_at(start);
		timer.wait();

		// execute main function of cmdScheduler/parent
		scheduler(start, stdInPipe);

		// cleanup
		close(stdInPipe[PIPE_WRITE]);
		usleep(1000000); 				// one second long grace period
		kill(pid, SIGTERM);				// cleanup child
	}else {
		//
		// CHILD
		//

		if(dup2(stdInPipe[PIPE_READ], STDIN_FILENO) == -1){
			std::cerr << "Failed to redirect stdin" << std::endl;
			exit(EXIT_FAILURE);
		}

		// close unnecessary file-descriptors
		close(stdInPipe[PIPE_READ]);
		close(stdInPipe[PIPE_WRITE]);

		// exec
        int result = execl("/bin/bash", "/bin/bash", (char*)0 );

		exit(result);
	}

    return 0;
}
