/**************************************************************
 * This file is part of the MPWide communication library
 *
 * Written by Derek Groen with thanks going out to Steven Rieder,
 * Simon Portegies Zwart, Joris Borgdorff, Hans Blom and Tomoaki Ishiyama.
 * for questions, please send an e-mail to: 
 *                                     djgroennl@gmail.com
 * MPWide is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published 
 * by the Free Software Foundation, either version 3 of the License, 
 * or (at your option) any later version.
 *
 * MPWide is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with MPWide.  If not, see <http://www.gnu.org/licenses/>.
 * **************************************************************/

#include <iostream>
#include <fstream>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <vector>
#include <string>
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

using namespace std;

#include "MPWide.h"
#define min(X,Y)   ((X) < (Y) ? (X) : (Y))

char prefix[80]    = "test";
bool add_suffix    = false;
char local_dir[80] = "/data/GreeM/Snapshot";
long buffer_size=10000000;

static vector <string> filelist;
static vector <string> allfiles;

/* Keeps a timestamp to track which files are older than the last check time. */
static time_t timestamp = 0;

int findfile(char* pattern) {
  for(int i=0;i<allfiles.size();i++) {
    if(allfiles[i].compare(pattern) == 0) { return i; }
  }
  return -1;
}

void add_file_to_list(string filename) {
  filelist.push_back(filename);
  allfiles.push_back(filename);
}

int list_size() {
  return filelist.size();
}

void remove_file_from_list() {
  filelist.erase(filelist.begin());
}

int check_new_files()
{
  DIR *dp;
  struct dirent *dirp;
  if((dp  = opendir(local_dir)) == NULL) {
    cout << "Error(" << errno << ") opening " << local_dir << endl;
    return errno;
  }

  while (dirp = readdir(dp)) {
    if(dirp->d_name[0] != '.' && strstr(dirp->d_name,"writing") == NULL) {

      char* writefname = (char*) malloc(sizeof(dirp->d_name)+sizeof(local_dir)+8);
      sprintf(writefname,"%s/%s.writing",local_dir,dirp->d_name);
 
      if(!access(writefname,W_OK)) {
        cout << "Writefile " << writefname << " has been found. " << dirp->d_name << " will not be copied yet." << endl;
	sleep(1);
      }
      else if(findfile(dirp->d_name)==-1) {
        int dup_found = 0;
        for(int i=0; i<filelist.size();i++) {
          if((filelist.at(0)).c_str()==dirp->d_name)
            dup_found++;
        }
        if(!dup_found) {
          add_file_to_list(string(dirp->d_name));
          cout << "Added " << string(dirp->d_name) << " to the list." << endl;
          cout << "List size = " << filelist.size() << endl;
        }
      }	
      free(writefname);
    }
  }

  closedir(dp);

  return 0;
}

/* Get the size of a file. */
long get_size(FILE* f) {
  fseek (f , 0 , SEEK_END);
  long size = ftell (f);
  rewind (f);
  return size;
}

void make_fname(char* a) {
  memcpy(a,local_dir,strlen(local_dir));
  memcpy(a+strlen(local_dir), prefix, strlen(prefix)+1);
}

int main(int argc, char** argv){
 
  string a;

  cout << "Maximum size = " << a.max_size() << endl;
  
  /* Initialize */

  int flag = atoi(argv[2]);
  int baseport = atoi(argv[3]);

  string host = (string) argv[1];

  /* Define 8 different streams */
  int size = 4;

  string *hosts = new string[size]; // = {host,host,host,host,host,host,host,host};
  int sports[size];    // = {6000,6001,6002,6003,6004,6005,6006,6007};
  int cports[size];
  int channel[size];

  for(int i=0; i<size; i++) {
    hosts[i] = host;
    sports[i] = baseport+i;
//    cports[i] = 6000+size+i;
    channel[i] = i;
  }

  //  MPW_Init(hosts,sports,cports,flags,size);
  MPW_Init(hosts,sports,size);
  delete [] hosts;

  if(flag) { //client mode
    while(true) {
      cout << "Client." << endl;

      char fname[256];

      check_new_files();

      int wait_cnt = 0;
      while(!filelist.size()){
        usleep(1000);
	check_new_files();
	wait_cnt++;
	if(wait_cnt>9) { wait_cnt = 0; cout << "."; }
      }

      cout << "filelist size = " << filelist.size() << endl;

      cout << local_dir << endl;
      cout << (filelist.at(0)).c_str() << endl;

      sprintf(fname,"%s/%s",local_dir,(filelist.at(0)).c_str());

      cout << "Trying to open the file: " << fname << endl;

      FILE* f = fopen(fname,"r");

      char* buf = (char*) malloc(buffer_size);

      uint32_t nfsize = htonl((uint32_t) get_size(f));
      long fsize = get_size(f);    

      cout << "fsize = " << fsize << endl;
      char fn[256];
      strcpy(fn,filelist.at(0).c_str());

      int *ch = {0};
      
      MPW_Send(fn,256,ch,1);
      MPW_Send((char*) &nfsize,4,ch,1);
      cout << "Starting main loop."<< endl;

      for(long i = 0; i<fsize;i+=buffer_size) {

        long read_len = min(fsize-i,buffer_size);
        long bytes_read = fread(buf,1,read_len,f);
        if(bytes_read != read_len) {
          cout << "Read error: Number of bytes read does not match expected amount." << endl;
          cout << "Read: " << bytes_read << " bytes. Expected: " << read_len << endl;
        }
//        MPW_Send(buf,read_len,0);
        long long int sendsize = 100;
        char *dummysend = new char[sendsize];
	MPW_SendRecv(buf, read_len, dummysend, sendsize, channel, size);
	delete [] dummysend;


      }

      remove_file_from_list();

      free(buf);
      fclose(f);
    }
  }
  else { //server mode
    while(true) {
      cout << "Server." << endl;

      char fname_temp[256]; 
      char fname[256];

      uint32_t rs_n = 0;

      long recvsize = 0;

      cout << "Receiving file header info." << endl;

      int *ch = {0};
      
      MPW_Recv(fname_temp,256,ch,1);
      MPW_Recv((char*) &rs_n,4,ch,1);

      sprintf(fname,"%s/%s",local_dir,fname_temp);

      cout << "Receiving file: " << fname << endl;

      FILE* f = fopen(fname,"w");

      recvsize = (long) ntohl(rs_n);

      char* buf = (char*) malloc(buffer_size);

      cout << "Starting main loop." << endl;

      for(long i = 0; i<recvsize; i += buffer_size) {

        long read_len = min(recvsize-i,buffer_size);

        long long int sendsize = 100;
        char *dummysend = new char[sendsize];
        MPW_SendRecv( dummysend, sendsize, buf, read_len, channel, size);
        delete [] dummysend;

        long bytes_written = fwrite(buf,1,read_len,f);
      }

      free(buf);
      fclose(f);
    }
  }

  MPW_Finalize();

  return 1;
}
