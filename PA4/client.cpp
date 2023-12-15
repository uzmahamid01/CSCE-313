#include <fstream>
#include <iostream>
#include <thread>
#include <sys/time.h>
#include <sys/wait.h>

#include "BoundedBuffer.h"
#include "common.h"
#include "Histogram.h"
#include "HistogramCollection.h"
#include "TCPRequestChannel.h"

// ecgno to use for datamsgs
#define ECGNO 1

using namespace std;


struct PVPair {
	int p;
	double v;
	
	PVPair (int _p, double _v) : p(_p), v(_v) {}
};

// TCPRequestChannel* create_new_channel (TCPRequestChannel* chan, int b) {
//     TCPRequestChannel* nchan = nullptr;

//     // request new channel
//     MESSAGE_TYPE m = NEWCHANNEL_MSG;
//     chan->cwrite(&m, sizeof(MESSAGE_TYPE));
    
//     char chname[b];
//     chan->cread(chname, sizeof(chname));

//     // open channel on client side
//     nchan = new TCPRequestChannel(chname, TCPRequestChannel::CLIENT_SIDE);

//     return nchan;
// }

void patient_thread_function (int n, int pno, BoundedBuffer* rb) {
    datamsg d(pno, 0.0, ECGNO);

    // populate data messages
    for (int i = 0; i < n; i++) {
        rb->push((char*) &d, sizeof(datamsg));
        d.seconds += 0.004;
    }
}

void file_thread_function (string fname, TCPRequestChannel* chan, int b, BoundedBuffer* rb) {
    // open receiving file and truncate appropriate number of bytes
    filemsg f(0, 0);
    char buf[sizeof(filemsg) + fname.size() + 1];
    memcpy(buf, &f, sizeof(filemsg));
    strcpy(buf + sizeof(filemsg), fname.c_str());
    
    chan->cwrite(buf, sizeof(buf));
	
    __int64_t flen;
    chan->cread(&flen, sizeof(__int64_t));
    
    FILE* myfile = fopen(("received/" + fname).c_str(), "wb");
    fseek(myfile, flen, SEEK_SET);
    fclose(myfile);

    // populate file messages
	__int64_t remainingbytes = flen;
    int buffer = b;
    int offset = 0;
    filemsg* fm = (filemsg*) buf;
    while (remainingbytes > 0) {
        if (buffer > remainingbytes) {
            buffer = remainingbytes;
        }
        offset = flen - remainingbytes;

        fm->offset = offset;
        fm->length = buffer;

        rb->push(buf, sizeof(buf));

        remainingbytes -= buffer;
    }
}

void worker_thread_function (TCPRequestChannel* chan, BoundedBuffer* rb, BoundedBuffer* sb, int b) {
    char buf[b];
    double result = 0.0;
    char recvbuf[b];
    
    while (true) {
        rb->pop(buf, sizeof(buf));
        MESSAGE_TYPE* m = (MESSAGE_TYPE*) buf;

        if (*m == DATA_MSG) {
            chan->cwrite(&buf, sizeof(datamsg));
            chan->cread(&result, sizeof(double));
			PVPair pv(((datamsg*) buf)->person, result);
            sb->push((char*) &pv, sizeof(PVPair));
        }
        else if (*m == FILE_MSG) {
            filemsg* fm = (filemsg*) buf;
            string fname = (char*) (fm + 1);
            chan->cwrite(&buf, (sizeof(filemsg) + fname.size() + 1));
			chan->cread(recvbuf, fm->length);

            FILE* myfile = fopen(("received/" + fname).c_str(), "rb+");
			fseek(myfile, fm->offset, SEEK_SET);
            fwrite(recvbuf, 1, fm->length, myfile);
            fclose(myfile);
        }
        else if (*m == QUIT_MSG) {
            chan->cwrite(m, sizeof(MESSAGE_TYPE));
            delete chan;
            break;
        }
    }
}

void histogram_thread_function (BoundedBuffer* rb, HistogramCollection* hc) {
	char buf[sizeof(PVPair)];
	while (true) {
		rb->pop(buf, sizeof(PVPair));
		PVPair pv = *(PVPair*) buf;
		if (pv.p <= 0) {
			break;
		}
		hc->update(pv.p, pv.v);
	}
}


int main (int argc, char *argv[]) {
    int n = 1000;	// default number of requests per "patient"
    int p = 10;		// number of patients [1,15]
    int w = 100;	// default number of worker threads
	int h = 20;		// default number of histogram threads
    int b = 30;		// default capacity of the request buffer
    string _ip_address;
    string port;
	int m = MAX_MESSAGE;	// default capacity of the message buffer
	string f = "";	// name of file to be transferred
    
    // read arguments
    int opt;
	while ((opt = getopt(argc, argv, "n:p:w:h:b:m:f:a:r:")) != -1) {
		switch (opt) {
			case 'n':
				n = atoi(optarg);
                break;
			case 'p':
				p = atoi(optarg);
                break;
			case 'w':
				w = atoi(optarg);
                break;
			case 'h':
				h = atoi(optarg);
				break;
			case 'b':
				b = atoi(optarg);
                break;
			case 'm':
				m = atoi(optarg);
                break;
			case 'f':
				f = optarg;
                break;
            case 'a':
                _ip_address = optarg;
                break;
            case 'r':
                port = optarg;
                break;
		}
	}
    bool filereq = (f != "");
    
	// fork and exec the server
    int pid = fork();
    if (pid == 0) {
        execl("./server", "./server", "-m", (char*) to_string(m).c_str(), nullptr);
    }
	
    
	// control overhead (including the control channel)
    TCPRequestChannel* chan = new TCPRequestChannel(_ip_address, port);
    BoundedBuffer request_buffer(b);
    BoundedBuffer response_buffer(b);
	HistogramCollection hc;

    // making histograms and adding to collection
    for (int i = 0; i < p; i++) {
        Histogram* h = new Histogram(10, -2.0, 2.0);
        hc.add(h);
    }


    // making worker channels
    TCPRequestChannel* wchans[w];
    for (int i = 0; i < w; i++) {
        wchans[i] = new TCPRequestChannel(_ip_address, port);
    }

	
	// record start time
    struct timeval start, end;
    gettimeofday(&start, 0);

    /* Start all threads here */
    thread patients[p];
    thread file;
    if (!filereq) {
        for (int i = 0; i < p; i++) {
            patients[i] = thread(patient_thread_function, n, (i+1), &request_buffer);
        }
    }
    else {
        file = thread(file_thread_function, f, chan, m, &request_buffer);
    }

    thread workers[w];
    for (int i = 0; i < w; i++) {
        workers[i] = thread(worker_thread_function, wchans[i], &request_buffer, &response_buffer, m);
    }

    thread hists[h];
    for (int i = 0; i < h; i++) {
        hists[i] = thread(histogram_thread_function, &response_buffer, &hc);
    }
	
	/* Join all threads here */
    if (!filereq) {
        for (int i = 0; i < p; i++) {
            patients[i].join();
        }
    }
    else {
        file.join();
    }

    for (int i = 0; i < w; i++) {
        MESSAGE_TYPE q = QUIT_MSG;
        request_buffer.push((char*) &q, sizeof(MESSAGE_TYPE));
    }

    for (int i = 0; i < w; i++) {
        workers[i].join();
    }
	
	for (int i = 0; i < h; i++) {
		PVPair pv(0, 0);
		response_buffer.push((char*) &pv, sizeof(PVPair));
	}
	
	for (int i = 0; i < h; i++) {
		hists[i].join();
	}

	// record end time
    gettimeofday(&end, 0);

    // print the results
	if (f == "") {
		hc.print();
	}
    int secs = ((1e6*end.tv_sec - 1e6*start.tv_sec) + (end.tv_usec - start.tv_usec)) / ((int) 1e6);
    int usecs = (int) ((1e6*end.tv_sec - 1e6*start.tv_sec) + (end.tv_usec - start.tv_usec)) % ((int) 1e6);
    cout << "Took " << secs << " seconds and " << usecs << " micro seconds" << endl;

	// quit and close control channel
    MESSAGE_TYPE q = QUIT_MSG;
    chan->cwrite ((char *) &q, sizeof (MESSAGE_TYPE));
    cout << "All Done!" << endl;
    delete chan;

	// wait for server to exit
	//wait(nullptr);
}