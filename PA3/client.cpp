#include <fstream>
#include <iostream>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "BoundedBuffer.h"
#include "common.h"
#include "Histogram.h"
#include "HistogramCollection.h"
#include "FIFORequestChannel.h"
#include <unistd.h>
#include <thread>
#include <vector>

using namespace std;
#define EGCNO 1

void patient_thread_function(int n, int p_no, BoundedBuffer* request_buffer){
    /*For a particular patient (let’s say p), it should generate n (number of
    requests—command line argument) data message objects requesting the 1st ECG value and
    place them in the request buffer. For these n data message requests, the time starts at 0.0 and
    increments to 0.004 for each n value.*/
    datamsg dmsg(p_no, 0.0,EGCNO);
    for(int i = 0; i < n; i++) {
        request_buffer->push((char*)&dmsg, sizeof(datamsg));
        dmsg.seconds += 0.004;
    }
}

void file_thread_function(string filename, BoundedBuffer* request_buffer,FIFORequestChannel* chan, int msg_buf) {
    //check if filename exists
    if(filename == "")
        return;

    //Create the file
    string source_file = "received/"+filename;
    //cout << "recvfname in file thread: " << source_file << endl;

    //Get the file length (refer to PA1).
    char buffer[1024];
    filemsg f(0,0);
    memcpy(buffer, &f, sizeof(f));
    strcpy(buffer+sizeof(f), filename.c_str());
    chan->cwrite(buffer, sizeof(f)+filename.size()+1);
    u_int64_t filelength;
    chan->cread(&filelength, sizeof(filelength));

    /*Create the target file in the received directory, and using fseek, set the offset to file
    length and close the file.*/
    FILE* target_file = fopen(source_file.c_str(), "wb");  //w+ or wb   both works but "wb" takes time dont know why
    //cout << "fopen file name: " << source_file << endl;
    if (target_file == nullptr) {
        perror("Error opening file");
    } else {
        fseek(target_file, filelength, SEEK_SET);
        fclose(target_file);
    }
    //Generate all the file messages 
    filemsg* fm = (filemsg*) buffer;
    u_int64_t remaining_length = filelength;
    //cout << "remlen: " << remaining_length << endl;
    while(remaining_length > 0) {
        fm->length = min(remaining_length,(u_int64_t) msg_buf);
        //and push them to request buffer queue
        request_buffer->push(buffer, sizeof(filemsg)+filename.size()+1);
        //increament the offset and decreament the remaining file length --- PA1
        fm->offset +=fm->length;
        remaining_length -= fm->length;
    }
    //cout << "done with the file thread" << endl;
}


void worker_thread_function(FIFORequestChannel* chan, BoundedBuffer* request_buffer, BoundedBuffer* response_buffer, int msg_buf){
    //cout << "starting worker threads" << endl;
    char buffer[1024];
    
    //initialize the double value of response pair
    double resp = 0;
    
    //char received_buffer[msg_buf];
    std::vector<char> received_buffer(msg_buf);

    while(true) {
        request_buffer->pop(buffer,1024);
        MESSAGE_TYPE*m = (MESSAGE_TYPE*) buffer;

        if(*m == DATA_MSG) {
            // write the datamsg to the FIFO channel
            chan->cwrite(buffer, sizeof(datamsg));

            // read the response from the FIFO channel (double value)
            chan->cread(&resp, sizeof(double));

            // create a pair object with the key as the patient ID and the value as the response
            std::pair<int, double> response_pair(((datamsg*)buffer)->person, resp);

            // push the pair to the response buffer
            response_buffer->push((char*)&response_pair, sizeof(std::pair<int, double>));
        }
        else if(*m ==FILE_MSG) {
            //cout << "Inside the FILE_MSG worker thread" << endl;

            filemsg* fm = (filemsg*) buffer;
            string filename = (char*) (fm + 1);
            int sz = sizeof(filemsg)+ filename.size()+1;
            //Write the message to the FIFO channel.
            chan->cwrite(buffer, sz);
            //Reads the response from the FIFO channel (char array of size = m (MAX_MESSAGE).
            //chan->cread(received_buffer, msg_buf);
            chan->cread(received_buffer.data(), msg_buf);

            string received_filename = "received/" + filename;
            //cout << "Inside the FILE_MSG worker thread recieved file: " << eceived_filename << endl;
            //Open the file using fopen() (use mode "rb+").
            FILE* target_file = fopen(received_filename.c_str(),"rb+"); 
            //Set the seek to the offset.
            fseek(target_file,fm->offset , SEEK_SET);
            //Write the response from step 2 to the opened file in step 3 using fwrite().
            //fwrite(received_buffer,1,fm->length, target_file);
            fwrite(received_buffer.data(),1,fm->length, target_file);
            //Close the file (you can use fclose()
            fclose(target_file);

        }
        else if(*m ==QUIT_MSG) {
            chan->cwrite(m, sizeof(MESSAGE_TYPE));
            delete chan;
            break;
        }
    }
}


void histogram_thread_function(BoundedBuffer* response_buffer, HistogramCollection* hc, int num_histograms) {
    //cout << "Starting histogram thread" << endl;
    char buffer[1024];

    while (true) {
        //pop an element from the response buffer
        response_buffer->pop(buffer, 1024);

        //check for a special signal to quit the thread
        if (buffer[0] == -1) {
            //cout << "Histogram thread received quit signal. Exiting..." << endl;
            break;
        }

        //since the element is a pair, extract the patient ID and response
        std::pair<int, double>* response_pair = reinterpret_cast<std::pair<int, double>*>(buffer);

        // bounds checking before invoking the update function
        if (response_pair->first >= 0 && response_pair->first < num_histograms) {  //num_histogram is number of histograms 
            //invoke the update function from the histogram collection object 
            hc->update(response_pair->first, response_pair->second);
        } else {
            //cerr << "Warning: Invalid patient ID " << response_pair->first << endl;
        }
    }
}

//Refer to how you created new fifo request channel in PA1
FIFORequestChannel* create_new_channel (FIFORequestChannel* main_channel){
    char cap[1024];
    MESSAGE_TYPE m = NEWCHANNEL_MSG;
    main_channel->cwrite(&m,sizeof(m));
    main_channel->cread(cap, 1024);
    FIFORequestChannel* new_channel = new FIFORequestChannel(cap, FIFORequestChannel::CLIENT_SIDE);
    return new_channel;
}

int main(int argc, char *argv[])
{
    int n = 15000;    //default number of requests per "patient"
    int p = 1;     // number of patients [1,15]
    int w = 200;    //default number of worker threads
    int h = 20;		// default number of histogram threads
    int b = 500; 	// default capacity of the request buffer, you should change this default
	int m = MAX_MESSAGE; 	// default capacity of the message buffer
    string f = "";
    
    bool file_request = false;

    int opt = -1;
    while((opt = getopt(argc,argv,"n:p:w:h:b:m:f:"))!=-1)
    {
        switch (opt)
        {
            case 'm':
                m = atoi(optarg);
                break;
            case 'n':
                n = atoi(optarg);
                break;
            case 'p':
                p = atoi(optarg);
                break;
            case 'h':
				h = atoi(optarg);
				break;  
            case 'b':
                b = atoi(optarg);
                break;
            case 'w':
                w = atoi(optarg);
                break;
            case 'f':
                f = optarg;
                file_request = true;
                break;
        }
    }
    
    int pid = fork();
    if (pid == 0) {
        execl("./server", "./server", "-m", (char*) to_string(m).c_str(), nullptr);
    }
    
	FIFORequestChannel* chan = new FIFORequestChannel("control", FIFORequestChannel::CLIENT_SIDE);
    BoundedBuffer response_buffer(b);
    BoundedBuffer request_buffer(b);
	HistogramCollection hc;

	//making histograms and adding to the histogram collection hc
	for(int i = 0; i < p; i++){
	    Histogram* h = new Histogram(10,-2.0,2.0);
	    hc.add(h);
    }

    /*Create w FIFORequestChannels
        a. I would recommend writing a separate function for creating a new channel and
            calling that function w times in a for loop*/
    std::vector<FIFORequestChannel*> worker_channel;
    for(int i = 0; i < w; i++) {
        worker_channel.push_back(create_new_channel(chan));
    }
	
	
    struct timeval start, end;
    gettimeofday (&start, 0);


    /* Start all threads here */
    /*For Data points request
        Create p threads and associate them to patient_thread_function*/
    vector<thread> patient;
    patient.resize(p);
    if(!file_request)
    {
        //thread patient [p];
        for(int i = 0; i < p;i++)
        {
            patient[i] = thread(patient_thread_function,n,i+1, &request_buffer );
        }
    }
    /*For file request
        Create 1 thread and associate it with file_thread_function*/
    thread filethread(file_thread_function, f, &request_buffer, chan, m);
    

    //Create w worker threads and associate them to worker_thread_function
    vector<thread> workers;
    workers.resize(w);
    for(int i = 0; i < w;i++)
    {
        workers[i] = thread(worker_thread_function, worker_channel[i],&request_buffer, &response_buffer, m);
    }
	
    //Create ‘h’ histogram threads and associate them to histogram_thread_function
    vector<thread> histogram;
    histogram.resize(h); 
    for(int i = 0; i < h; i++)
    {
        histogram[i] = thread(histogram_thread_function, &response_buffer, &hc, n);
    }
	
	/* Join all threads here in the same order*/
    //join patients
	if(!file_request)
    {
        for(int i = 0; i < p;i++)
        {
            patient[i].join();
        }
    }
    //cout << "Patient threads finished" <<endl;

    //join file
	filethread.join();
    //cout << "Patient/file threads finished" <<endl;

    //send quit messages w times to signal the end of processing to worker threads
    for(int i = 0; i < w;i++)
    {
        MESSAGE_TYPE q = QUIT_MSG;
        request_buffer.push ((char*) &q,sizeof(q));
    }

    //join workers
    for(int i = 0; i < w;i++)
    {
        workers[i].join();
    }

    //cout << "Worker threads finished" <<endl;

    // Push special pairs to signal the end of processing to histogram threads
    for (int i = 0; i < h; ++i) {
        std::pair<int, double> end_signal(-1, -1);
        response_buffer.push((char*)&end_signal, sizeof(std::pair<int, double>));
    }

    //join histogram
    for(int i = 0; i < h;i++)
    {
        histogram[i].join();
    }
    
    // record end time
    gettimeofday (&end, 0);

	// print the results
	if (f == "") {
		hc.print();
	}

    int secs = (end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec)/(int) 1e6;
    int usecs = (int)(end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec)%((int) 1e6);
    cout << "Took " << secs << " seconds and " << usecs << " micro seconds" << endl;

    // quit and close control channel
    MESSAGE_TYPE q = QUIT_MSG;
    chan->cwrite ((char *) &q, sizeof (MESSAGE_TYPE));
    cout << "All Done!!!" << endl;
    delete chan;

    // wait for server to exit
	wait(nullptr);

    return 0;
    
}

