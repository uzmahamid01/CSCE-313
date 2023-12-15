/*
    Original author of the starter code
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date: 2/8/20
    
    Please include your Name, UIN, and the date below
    Name: Uzma Hamid
    UIN: 332009674
    Date: 18 september 2023
*/


#include "common.h"
#include "FIFORequestChannel.h"
#include <fstream> 
#include <iostream>
#include <unistd.h>

using namespace std;

int main (int argc, char *argv[]) {
    int opt;
    int p = -1;
    double t = -1;
    int e = -1;
    int m = MAX_MESSAGE;
    bool new_channel = false;
    vector<FIFORequestChannel* > channels;
    
    string filename = "";
    
    while ((opt = getopt(argc, argv, "p:t:e:f:m:c")) != -1) {
        switch (opt) {
            case 'p':
                p = atoi(optarg);
                break;
            case 't':
                t = atof(optarg);
                break;
            case 'e':
                e = atoi(optarg);
                break;
            case 'f':
                filename = optarg;
                break;
            case 'm':
                m = atoi(optarg);
                break;
            case 'c':
                new_channel = true;
                break;
        }
    }

    if (fork() == 0) {
        std::cout << "Running server as a child" << endl;
        const char* args[] = {"./server", "-m", to_string(m).c_str(), NULL};
        if (execvp(args[0], const_cast<char* const*>(args)) < 0) {
            exit(0);
        }
    }

    FIFORequestChannel cont_chan("control", FIFORequestChannel::CLIENT_SIDE);
    channels.push_back(&cont_chan);

    if (new_channel) {
        MESSAGE_TYPE mtw = NEWCHANNEL_MSG;
        cont_chan.cwrite(&mtw, sizeof(MESSAGE_TYPE));

        char new_channel[MAX_MESSAGE];
        cont_chan.cread(new_channel, sizeof(new_channel));

        FIFORequestChannel* new_chan = new FIFORequestChannel(new_channel, FIFORequestChannel::CLIENT_SIDE);
        channels.push_back(new_chan);
        cout << "New channel created with name: " << new_channel << endl;
    }

    FIFORequestChannel &chan = *(channels.back());

    if (p >= 1 && p <= 15 && t != -1 && e != -1) {
        char buf[MAX_MESSAGE];
        datamsg x(p, t, e);
        memcpy(buf, &x, sizeof(datamsg));
        chan.cwrite(buf, sizeof(datamsg));
        double reply;
        chan.cread(&reply, sizeof(double));
        cout << "For person " << p << ", at time " << t << ", the value of ecg " << e << " is " << reply << endl;
    } else if (!filename.empty()) {
        filemsg fm(0, 0);
        string fname = filename;
        int len = sizeof(filemsg) + (fname.size() + 1);
        char* buf2 = new char[len];
        memcpy(buf2, &fm, sizeof(filemsg));
        strcpy(buf2 + sizeof(filemsg), fname.c_str());
        chan.cwrite(buf2, len);

        __int64_t fileLength = 0;
        chan.cread(&fileLength, sizeof(__int64_t));

        char* buf3 = new char[m];
        int remainingBytes = fileLength;
        int offset = 0;

        ofstream outFile("received/" + fname, ios::binary | ios::trunc); // Open the file in trunc mode to overwrite it

        while (remainingBytes > 0) {
            filemsg* file_req = (filemsg*)buf2;
            file_req->offset = offset;
            file_req->length = min(m, remainingBytes);
            chan.cwrite(buf2, len);

            int bytesRead = chan.cread(buf3, file_req->length);

            if (bytesRead != file_req->length) {
                cerr << "Error: Received " << bytesRead << " bytes, expected " << file_req->length << " bytes." << endl;
                break;
            }

            outFile.write(buf3, file_req->length);

            offset += file_req->length;
            remainingBytes -= file_req->length;
        }
        outFile.close();
        delete[] buf2;
        delete[] buf3;
    } else if (p >= 1 && p <= 15) {
        // Request for 1000 data points
        ofstream outFile("received/x1.csv", ios::binary | ios::trunc);

        t = 0.00;
        for (int i = 0; i < 1000; i++) {
            char buf[MAX_MESSAGE];
            datamsg x1(p, t, 1);
            datamsg x2(p, t, 2);

            memcpy(buf, &x1, sizeof(datamsg));
            chan.cwrite(buf, sizeof(datamsg));
            double reply1;
            chan.cread(&reply1, sizeof(double));

            memcpy(buf, &x2, sizeof(datamsg));
            chan.cwrite(buf, sizeof(datamsg));
            double reply2;
            chan.cread(&reply2, sizeof(double));

            outFile << t << "," << reply1 << "," << reply2 << endl;
            t += 0.004;
        }

        outFile.close();
    } else {
        cout << "Invalid request: Neither data point nor file transfer requested." << endl;
    }

    //close and delete the new channel
    if (new_channel) {
        delete channels.back();
        channels.pop_back();
        cout << "Closing and deleting the new channel" << endl;
    }
    
    cout << "Sending QUIT_MSG to close the channel" << endl;

    // closing the channel
    MESSAGE_TYPE mt = QUIT_MSG;
    chan.cwrite(&mt, sizeof(MESSAGE_TYPE));
    cont_chan.cwrite(&mt, sizeof(MESSAGE_TYPE));
    
    return 0;
}

//another approach
// if(p >=1 && p <= 15 && t != -1 && e != -1) {
//         // example data point request
//         //cout << "Sending data point request" << endl;
//         char buf[MAX_MESSAGE]; // 256
//         datamsg x(p, t, e);  //(patient value,time,ecg value)  //hard code to users request value 
        
//         memcpy(buf, &x, sizeof(datamsg));
//         chan.cwrite(buf, sizeof(datamsg)); // question
//         double reply;
//         chan.cread(&reply, sizeof(double)); //answer
//         std::cout << "For person " << p << ", at time " << t << ", the value of ecg " << e << " is " << reply << endl;
        
//     }
//     else if (p!=-1){
//         //loop over first 1000 lines
//         //send the request for ecg 1
//         //send request for ecg 2
//         //write line to received/x1.csv
//         //std::cout << "entered the 1000 data loop" << endl;
//         ofstream outFile("received/x1.csv", ios::trunc);
//         if (!outFile.is_open()) {
//             cerr << "Error" << endl;
//             return -1;
//         }

//         cout << "opened the file successfully" << endl;
//         t = 0.00;
//         for (int i = 0; i < 1000; i++) {
//             //cout << "entered the loop 2" << endl;
//             char buf[MAX_MESSAGE]; // 256
//             //datamsg x(p, t, e);  //(patient value,time,ecg value)
//             datamsg x1(p, t, 1); // Request for ECG channel 1
//             datamsg x2(p, t, 2); // Request for ECG channel 2
                
//             // Send the request for ECG channel 1
//             //cout << "Sending request for ECG channel 1" << endl;
//             memcpy(buf, &x1, sizeof(datamsg));
//             chan.cwrite(buf, sizeof(datamsg)); // question
//             double reply1;
//             chan.cread(&reply1, sizeof(double)); //answer   


//             // Send the request for ECG channel 2
//             //cout << "Sending request for ECG channel 2" << endl;
//             memcpy(buf, &x2, sizeof(datamsg));
//             chan.cwrite(buf, sizeof(datamsg));
//             double reply2;
//             chan.cread(&reply2, sizeof(double));

            
//             outFile << t << "," << reply1 << "," << reply2 << endl;
//             t += 0.004;
//         }
//         cout << "closing the file" << endl;
//         outFile.close();
//     }


//    //requesting a file
//     filemsg f (0,0);  // special first message to get file size
//     int len = sizeof (filemsg) + filename.size() + 1; // extra byte for NULL
//     char* buf = new char [len];
//     memcpy (buf, &f, sizeof(filemsg));
//     strcpy (buf + sizeof (filemsg), filename.c_str());
//     chan.cwrite (buf, len);
//     __int64_t filesize;
//     chan.cread (&filesize, sizeof (__int64_t));
//     cout << "File size: " << filesize << endl;

//     //int transfers = ceil (1.0 * filesize / MAX_MESSAGE);
//     filemsg* file_req = (filemsg*) buf;
//      __int64_t remainingBytes = filesize;
//     string outfilepath = string("received/") + filename;

//     //lets Open output file for writing
//     FILE* outfile = fopen(outfilepath.c_str(), "wb");
//     if (outfile != nullptr) {
//         char* buf3 = new char[MAX_MESSAGE];
//         if (buf3 != nullptr) {
//             while (remainingBytes > 0) {
//                 file_req->length = (int)min(remainingBytes, (__int64_t)MAX_MESSAGE);
//                 chan.cwrite(buf, len);
//                 chan.cread(buf3, MAX_MESSAGE);
//                 size_t bytes_written = fwrite(buf3, 1, file_req->length, outfile);

//                 if (bytes_written < static_cast<size_t>(file_req->length)) {
//                     cerr << "Error writing to file." << endl;
//                     // Close the file
//                     fclose(outfile);
//                     delete[] buf3;
//                     delete[] buf;
//                     return -1;
//                 }

//                 remainingBytes -= file_req->length;
//                 file_req->offset += file_req->length;
//             }

//             // Close the output file
//             fclose(outfile);
//             delete[] buf3;
//         } else {
//             cerr << "Failed to allocate memory for recv_buffer." << endl;
//             // Close the file
//             fclose(outfile);
//             delete[] buf;
//             return -1;
//         }
//     } else {
//         cerr << "Failed to open the output file for writing." << endl;
//         delete[] buf;
//         return -1;
//     }
