#include <iostream>
#include <pthread.h>
#include <vector>
#include <string>
#include <regex>
#include <memory>
#include <map>
#include <chrono>
#include <csignal>
#include <chrono>
#include <sys/time.h>

#include "minisat/core/SolverTypes.h"
#include "minisat/core/Solver.h"

using namespace std;
using namespace Minisat;

struct ThreadArg{
    int v;
    vector < int > edges;
    string results;
    std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
    std::chrono::time_point<std::chrono::high_resolution_clock> end_time;
    std::chrono::microseconds duration_time;
};

long threadCalc(ThreadArg* args){
    auto start = std::chrono::time_point_cast<std::chrono::milliseconds>(args->start_time).time_since_epoch().count();
    auto end = std::chrono::time_point_cast<std::chrono::milliseconds>(args->end_time).time_since_epoch().count();
    long time = end - start;

    return time;
}

int vertex;
vector < int > edges;

int get_v(string input) {
  regex pattern("^[V]\\s(\\d+)(\\s)*$");
  smatch matches;
  return (regex_match(input, matches, pattern)) ? stoi(matches[1]) : -1;
}

vector < int > get_e(string input) {
  vector < int > temp_edge;
  regex pattern("\\d+");
  smatch matches;
  while (regex_search(input, matches, pattern)) {
    for (string match: matches) {
      temp_edge.push_back(stoi(match));
    }
    input = matches.suffix().str();
  }
  return temp_edge;
}

// CNF-SAT
void* CNFSAT(void* args) {
  //cout << "CNFSAT Thread started" << endl;
  ThreadArg* inputArgs = static_cast<ThreadArg*>(args);
  vertex = inputArgs->v;
  edges = inputArgs->edges;

  // start timer for runtime calculation
  inputArgs->start_time = std::chrono::high_resolution_clock::now();
  auto start_time = std::chrono::high_resolution_clock::now();
  std::chrono::time_point<std::chrono::high_resolution_clock> timer_start = std::chrono::high_resolution_clock::now();
  
  // time out setting in us (60,000,000)us = 1 minute 
  int timeout = 500000;

  std::unique_ptr < Solver > solver(new Solver());
  for (int counter = 1; counter < vertex + 1; counter++) {
    // every loop check time
    auto timer_now = std::chrono::high_resolution_clock::now();
    auto elapsed_seconds = std::chrono::duration_cast<std::chrono::microseconds>(timer_now - timer_start);
    if (elapsed_seconds.count() >= timeout) {
            //cout << "timeout set for : " << timeout << "seconds and current timer is running for " << elapsed_seconds.count() << endl;
            inputArgs->results = "CNF-SAT-VC: timeout";
            pthread_exit(nullptr);
    }
    
    vec < Lit > temp;
    // Crete new variables for all
    for (int i = 0; i < counter; i++) {
      for (int j = 0; j < vertex; j++) {
        temp.push(mkLit(solver -> newVar()));
      }
    }
    // C1. At least one vertex is the ith vertex in the vertex cover
    for (int i = 0; i < counter; i++) {
      vec < Lit > c1;
      for (int j = 0; j < vertex; j++) {
        c1.push(temp[(i * vertex) + j]);
      }
      solver -> addClause(c1);
      c1.clear();
    }
    // C2. No one vertex can appear twice in a vertex cover.
    for (int i = 0; i < vertex; i++) {
      for (int j = 0; j < counter - 1; j++) {
        for (int l = j + 1; l < counter; l++) {
          solver -> addClause(~temp[(j * vertex) + i], ~temp[(l * vertex) + i]);
        }
      }
    }
    // C3. No more than one vertex appears in the mth position of the vertex cover
    for (int i = 0; i < counter; i++) {
      for (int j = 0; j < vertex - 1; j++) {
        for (int l = j + 1; l < vertex; l++) {
          solver -> addClause(~temp[(i * vertex) + j], ~temp[(i * vertex) + l]);
        }
      }
    }
    // C4. Every edge is incident to at least one vertex in the vertex cover
    for (size_t i = 1; i < edges.size(); i += 2) {
      vec < Lit > c4;
      for (int j = 0; j < counter; j++) {
        c4.push(temp[(j * vertex) + (edges[i - 1] - 1)]);
        c4.push(temp[(j * vertex) + (edges[i] - 1)]);
      }
      solver -> addClause(c4);
      c4.clear();
    }
    
    /* Attempted optimization -- not successful
    // C5. Avoid reciprocal entries into vertex cover (full order relationship)
    for (int i = 0; i < vertex; i++){
        for (int j = 0; j < counter -1; j++){
            for (int x = i+1; x < vertex; x++){
                for (int y = 0; y < j; y++){
                    //a[i][j] v ~ a[x][y]
                    printf("a[%d][%d]v~a[%d][%d] || ",i,j,x,y);
                    cout << endl;
                    solver -> addClause(temp[(i * vertex) + j], ~temp[(x * vertex) + y]); // did i access a[i][j] v ~a[x][y] correctly?
                    // a[x][y] is supposed to be a[{i+1 to vertex}][{0 to j-1}]
                }
            }
        }
    }
    */

    // Result
    vector < int > result;
    if (solver -> solve()) {
      for (int i = 0; i < counter; i++) {
        for (int j = 0; j < vertex; j++) {
          if (!toInt(solver -> modelValue(temp[(i * vertex) + j]))) {
            result.push_back(j + 1);
          }
        }
      }
      sort(result.begin(), result.end());
      //copy(result.begin(), result.end() - 1, ostream_iterator < int > (cout, " "));
      //cout << result.back() << endl;
      string resultString;
      
      resultString += "CNF-SAT-VC: ";

      for (size_t i = 0; i < result.size(); ++i) {
        resultString += std::to_string(result[i]);

        // Add space if not the last element
        if (i < result.size() - 1) {
            resultString += ",";
        }
      }
      
      inputArgs->results = resultString;

      // stop timer for runtime calculation
      inputArgs->end_time = std::chrono::high_resolution_clock::now();
      auto end_time = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
      inputArgs->duration_time = duration;
      pthread_exit(nullptr); //required for threads, instead of return 0
    }
    solver.reset(new Solver());
  }
  return nullptr;
}

// APPROX-VC1
void* VC1(void* args) {
    ThreadArg* inputArgs = static_cast<ThreadArg*>(args);
    
    vector<int> edgesVC1 = inputArgs->edges;
    vector<int> edgeCopy = edgesVC1;
    vector<int> VC1;
    map<int, vector<int> > edgeMap;

    // start timer for runtime calculation
    auto start_time = std::chrono::high_resolution_clock::now();
    inputArgs->start_time = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < edgeCopy.size() - 1; i+=2) {
        edgeMap[edgeCopy[i]].push_back(edgeCopy[i + 1]);
        edgeMap[edgeCopy[i + 1]].push_back(edgeCopy[i]);
    }

    // find highest edge, add to vector VC
    while (!edgeMap.empty()){
        size_t maxSize = 0;
        int maxKey = 0;
        vector<int> maxVal;
        for (auto it = edgeMap.begin(); it != edgeMap.end(); ++it){
            int key = it->first;
            vector<int> valVC1 = it->second;
            if (valVC1.size() > maxSize){
                maxSize = valVC1.size();
                maxKey = key;
                maxVal = valVC1;
            }
        }
        VC1.push_back(maxKey);


        // loop through edges attached to highest incident edge and remove from map
        for(auto& u: maxVal){
            auto& edgesList = edgeMap[u];
            edgesList.erase(std::remove(edgesList.begin(), edgesList.end(), maxKey), edgesList.end());

            // If the incident edge list becomes empty, remove it from the map
            if (edgesList.empty()) {
                edgeMap.erase(u);
            }
        }
        
        edgeMap.erase(maxKey);

    }
    // organize VC and return string output
    std::sort(VC1.begin(), VC1.end());
    std::string result;
    result += "APPROX-VC-1: ";
    for (size_t i = 0; i < VC1.size(); ++i) {
        result += std::to_string(VC1[i]);
        if (i < VC1.size() - 1) {
            result += ",";
        }
    }
    inputArgs->results = result;

    // stop timer for runtime calculation
    inputArgs->end_time = std::chrono::high_resolution_clock::now();
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    inputArgs->duration_time = duration;
    pthread_exit(nullptr);

}

// APPROX-VC2
void* VC2(void* args) {
    //cout << "VC2 Thread started" << endl;
    ThreadArg* inputArgs = static_cast<ThreadArg*>(args);
    vector<int> edgesVC2 = inputArgs->edges;
    vector<int> VC2;

    // start timer for runtime calculation
    inputArgs->start_time = std::chrono::high_resolution_clock::now();
    auto start_time = std::chrono::high_resolution_clock::now();

    while (!edgesVC2.empty()){
        // store values of edge <x,y>
        int vec1 = edgesVC2[0];
        int vec2 = edgesVC2[1];
        // add x and y to VC
        VC2.push_back(edgesVC2[0]);
        VC2.push_back(edgesVC2[1]);
        // remove x and y
        edgesVC2.erase(edgesVC2.begin());
        edgesVC2.erase(edgesVC2.begin());        
        
        vector<int> eraseVec;
        for(size_t i = 0; i < edgesVC2.size(); i++){
            if(edgesVC2[i] == vec1 || edgesVC2[i] == vec2){
                if(i == 0){
                    eraseVec.push_back(i);
                    eraseVec.push_back(i+1);
                } else if(i == 1){
                    eraseVec.push_back(i);
                    eraseVec.push_back(i-1);
                } else if (i >= 2 && i%2 == 0){
                    eraseVec.push_back(i);
                    eraseVec.push_back(i + 1);
                } else if (i >= 2 && i%2 == 1){
                    eraseVec.push_back(i);
                    eraseVec.push_back(i-1);
                }
            }
        }
        std::sort(eraseVec.begin(), eraseVec.end());
        for (auto it = eraseVec.rbegin(); it != eraseVec.rend(); ++it) {
        edgesVC2.erase(edgesVC2.begin() + *it);
        }
    }
    // organize VC and return string output
    std::sort(VC2.begin(), VC2.end());
    std::string result;
    result += "APPROX-VC-2: ";
    for (size_t i = 0; i < VC2.size(); ++i) {
        result += std::to_string(VC2[i]);
        if (i < VC2.size() - 1) {
            result += ",";
        }
    }
    inputArgs->results = result;

    // stop timer for runtime calculation
    inputArgs->end_time = std::chrono::high_resolution_clock::now();
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    inputArgs->duration_time = duration;
    pthread_exit(nullptr);
}


void* processInput(string userInput) {
  switch (userInput[0]) {
  case 'V':
    vertex = get_v(userInput);
    break;
  case 'E':
    edges = get_e(userInput);
    pthread_t CNFSAT_Thread, VC1_Thread, VC2_Thread;
    
    ThreadArg CNFSAT_Args = {vertex, edges, "CNF-SAT-VC:"};
    ThreadArg VC1_Args = {vertex, edges, "APPROX-VC-1:"};
    ThreadArg VC2_Args = {vertex, edges, "APPROX-VC-2:"};

    // create thread for each algorithm
    pthread_create(&CNFSAT_Thread, nullptr, CNFSAT, &CNFSAT_Args);
    pthread_create(&VC1_Thread, nullptr, VC1, &VC1_Args);
    pthread_create(&VC2_Thread, nullptr, VC2, &VC2_Args);

    struct timespec ts = {0,0};
    //add time to timeout
    ts.tv_sec += 120;

    // wait for threads to finish
    pthread_join(VC1_Thread, nullptr);
    pthread_join(VC2_Thread, nullptr);
    pthread_join(CNFSAT_Thread, nullptr);

    cout << CNFSAT_Args.results << endl;
    //cout << "CNF-SAT-VC runtime: " << CNFSAT_Args.duration_time.count() << "us" << endl;

    cout << VC1_Args.results << endl;
    //cout << "APPROX-VC-1 runtime: " << VC1_Args.duration_time.count() << "us" << endl;

    cout << VC2_Args.results << endl;
    //cout << "APPROX-VC-2 runtime: " << VC2_Args.duration_time.count() << "us" << endl; 

    break;
  }
  return nullptr;
}


void* IO(void* args) {
    while (!cin.eof()) {
        string userInput;
        getline(cin, userInput);
        processInput(userInput);
    }
    return nullptr;
}

int main(int argc, char ** argv) {
    pthread_t io_Thread;
    pthread_create(&io_Thread, nullptr, IO, nullptr);
    pthread_join(io_Thread, nullptr);
    return 0;
}