#include <string>
#include <vector>

#include <iostream>
#include <fstream>

#include <ctime>
#include <unistd.h>

#include <sys/types.h>
#include <sys/resource.h>
#include <signal.h>

#include <dirent.h>
#include <pwd.h>

using namespace std;

//Input struct to maintain what the user input in the command lines
struct Input{
	const char* l;
	const char* c;
	bool d;
	
	//Setting default values
	Input(int argc, char *argv[]){
		/*
		l = "/var/log/phunt.log";
		c = "/etc/phunt.conf";
		d = false;
		*/

		l = "phunt.log";
		c = "phunt.conf";
		d = false;
		
		//Parses the input to check if any relavent inputs were given and stores them
		for (int i = 1; i < argc; i++){
			if ((string)argv[i] == "-l"){
				i++;
				l = argv[i];
				
			} else if ((string)argv[i] == "-c"){
				i++;
				c = argv[i];
				
			} else if ((string)argv[i] == "-d"){
				d = true;
			} 
		}
	};
};


//Struct which stores all the information for each individual rule
struct Command{
	string action;
	string type;
	string param;
	Command(string action, string type, string param){
		this->action = action;
		this->type = type;
		this->param = param;
	}	
};

//Struct which holds all the relevat information for each process
struct ProcData{
	int id;
	int size;
	string user;
	string path;

	ProcData(int id, int size, string user, string path){
		this->id = id;
		this->size = size;
		this->user = user;
		this->path = path;
	}
};

//Parses through the config and grabs each rule and adds them to a vector of such rules.
int parseConf(vector<Command>* list, const char* filename){
	ifstream confFile(filename);

	if (confFile.fail()){
		return -1;
	}
	
	string line;
	
	while(getline(confFile, line)){
		if (line[0] == '#' || line.empty()){
			continue;
		}	
		
		int firstSpace = line.find(" ");
		int lastSpace = line.find(" ", firstSpace + 1);
		
		string action = line.substr(0, firstSpace);
		string type = line.substr(firstSpace + 1, lastSpace - firstSpace - 1);
		string param = line.substr(lastSpace + 1, line.length() - lastSpace);
		
		list->push_back(Command(action, type, param));
	}
	
	return 0;
}

//This is just used to format the log entries
string logCreator(string event){
	time_t now = time(0);
   
	string dt = ctime(&now);
	dt = dt.substr(4, dt.length()- 10);

	return (dt.substr(0, 3) + dt.substr(4, dt.length()) + " ubuntu phunt: " + "\t" + event);
}

//Uses password files to get the user name from the user id
string getNameFromUID(int uid){
	struct passwd *pwd = getpwuid(uid);

	return pwd->pw_name;
}

//Reads each numerical directory in /proc/ and return each id
vector<string> getProcList(){
	vector<string> procList;

	DIR *dp = opendir("/proc");

	if (dp != NULL){
		struct dirent *dirp;

		while((dirp = readdir(dp))){
			
			if (atoi(dirp->d_name) > 0){
				procList.push_back(dirp->d_name);
			}		
		}	
	}

	return procList;
}

//Get a processes size based on its id
int getProcSize(string id){
	string sizePath = "/proc/" + id + "/status";
	
	ifstream statusFile(sizePath);

	if(statusFile.fail()){
		//cout << "NotFound" << endl;
	}

	string line;

	//Parses through and gets the size and changes it into mB
	while(getline(statusFile, line)){
		if (line.substr(0,7) == "VmSize:"){
			string sizeStr = line.substr(7);
			int sizeTypeEnd = sizeStr.find("B");

			char type = sizeStr[sizeTypeEnd-1];

			string size = sizeStr.substr(0, sizeTypeEnd - 2);

			double sizeDob = stod(size);

			if (type == 'k'){
				sizeDob = sizeDob/1024;
			}

			return sizeDob;
		}		
	}

	return 0;
}

//Get a process' user based on its id
string getProcUser(string id){
	string sizePath = "/proc/" + id + "/status";
	
	ifstream statusFile(sizePath);

	if(statusFile.fail()){
		//cout << "NotFound" << endl;
	}

	string line;

	//Parses through status to get the uid and gets a name based on it
	while(getline(statusFile, line)){
		if (line.substr(0,4) == "Uid:"){
			string uid = line.substr(5);

			string name = getNameFromUID(stoi(uid));

			return name;
		}
	}

	return "0";
}

//Returns the path of the proc 
string getProcPath(string id){
	string pathPath = "/proc/" + id + "/exe";
	
	char procPath[PATH_MAX + 1] = {0};

	readlink(pathPath.c_str(), procPath, 1024);

	return string(procPath);
}

//Gets the all the peices of a proc and creates a ProcData struct to hold all that information
ProcData getProcInfo(string id){
	int idd = stoi(id);
	
	double size = getProcSize(id);
	string user = getProcUser(id);
	string path = getProcPath(id);

	return (ProcData(idd, size, user, path));
}

//Get the state of the proc
//This is mostly used to check if its suspeded or running
//Its another way to make sure that a process was dealth with properly
char getProcState(int id){
	string statPath = "/proc/" + to_string(id) + "/status";
	
	ifstream statFile(statPath);

	if(statFile.fail()){
		//cout << "NotFound" << endl;
	}

	string line;

	while(getline(statFile, line)){
		if (line.substr(0,6) == "State:"){
			string uid = line.substr(5);
			
			return line[7];
		}
	}

	return '0';
}

//Depending on the action this will affect processes
string affectProcs(ProcData proc, string action){

	//To kill a process try and kill it
	//repeteadly check it it still exists
	//If it still does try to kill it
	//Check the state to see if its dead or a zombie and stop if so
	if (action == "kill"){
		while(kill(proc.id, 0) == 0){
			kill(proc.id, SIGKILL);

			char state = getProcState(proc.id);

			if (state == 'Z' || state == 'X'){
				break;
			}
		}

		return "Killed process " + to_string(proc.id);

	} else if (action == "nice"){

		//Repeatedly change the nice value to -20 as long as the process doesn't have a nice value of -20
		while(getpriority(PRIO_PROCESS, proc.id) != -20){
			setpriority(PRIO_PROCESS, proc.id, -20);
		}

		string temp = "Process " + to_string(proc.id);

		return temp + " set to -20 priority";

	} else if (action == "suspend"){

		//Suspend the process and make sure that its not still running
		while(getProcState(proc.id) == 'R'){
			kill(proc.id, SIGTSTP);
		}
		
		return "Suspending process " + to_string(proc.id);

	}

	return "";
}

//For each proc try to apply each rule onto it
//This is done top down so things higher in the conf file have priority
string applyCommands(vector<Command>* blacklist, ProcData proc){
	for (int i = 0; i < blacklist->size(); i++){
		if (blacklist->at(i).type == "user"){

			//Checks if the user is in the same user to blacklist
			if (blacklist->at(i).param == proc.user){
				return affectProcs(proc, blacklist->at(i).action);
			}

		} else if (blacklist->at(i).type == "path"){	

			//Checks if the blacklisted path is a part of the process's path
			if (proc.path.substr(0, blacklist->at(i).param.length()) == blacklist->at(i).param && !proc.path.empty()){
				return affectProcs(proc, blacklist->at(i).action);	
			}

		} else if (blacklist->at(i).type == "memory"){

			//Checks if the memory being used is greater than the one blacklisted
			if (stod(blacklist->at(i).param) <= proc.size){
				return affectProcs(proc, blacklist->at(i).action);	
			}
		}
	}

	return "";
}

int main ( int argc, char *argv[] ) {
	//Initialize your input and get your blacklist ready

	Input input(argc, argv);
	vector<Command>* blacklist = new vector<Command>();
	
	//Open the log file stream
	ofstream logFile;
	logFile.open (input.l, ofstream::app);
	
	if (logFile.fail()){
		cout << "Log file doesn't exist." << endl;
		return -1;
	}
	
	logFile << logCreator("phunt startup (" + std::to_string(::getpid()) + ")") << endl;
	
	logFile << logCreator("opened logfile " + (string)input.l) << endl;
	
	//populate the blacklist with a set of rules
	if (parseConf(blacklist, input.c) != 0){
		logFile << logCreator("Configure file not found.") << endl;	
		cout << "Configure file doesn't exist." << endl;
		return -1;
	}
	
	logFile << logCreator("parsing configuration " + (string)input.c) << endl;	

	//This is here in case i need to make it run in a loop (for deamonizing)
	while (true){
		//Get each proc current procId
		logFile << logCreator("Acquiring all procs") << endl;	
		vector<string> procIds = getProcList();

		vector<ProcData> procList;

		logFile << logCreator("Parsing all procs") << endl;

		//Get data for each active proc
		for (int i = 0; i < procIds.size(); i++){
			procList.push_back(getProcInfo(procIds[i]));
		}

		logFile << logCreator("Removing any violating procs") << endl;

		//For each id try and apply each rule to it
		//Logg anything action that was done
		for (int i = 0; i < procList.size(); i++){
			string effect = applyCommands(blacklist, procList[i]);

			if (!effect.empty()){
				logFile << logCreator(effect) << endl;
			}
		}
		
		logFile << logCreator("Sleeping for 5 seconds") << endl;	

		sleep(5);
	}

	logFile.close();

	cout << "Log file had an error." << endl;
	
	return 0;
}
