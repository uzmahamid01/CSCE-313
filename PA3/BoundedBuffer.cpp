#include "BoundedBuffer.h"
#include <cassert>

using namespace std;

BoundedBuffer::BoundedBuffer(int _cap) : cap(_cap) {
    // modify as needed
}

BoundedBuffer::~BoundedBuffer() {
    // modify as needed
}

void BoundedBuffer::push(char* msg, int size) {
    // 1. Convert the incoming byte sequence given by msg and size into a vector<char>
    vector<char> dataVector(msg, msg + size);

    // 2. Wait until there is room in the queue (i.e., queue length is less than cap)
    unique_lock<mutex> lock(m);
    slot_avail.wait(lock, [this] { return static_cast<int>(q.size()) < cap; });

    // 3. Then push the vector at the end of the queue
    q.push(dataVector);
    
    // 4. Wake up threads that were waiting for push
    data_avail.notify_one();
}

int BoundedBuffer::pop(char* msg, int size) {
    // 1. Wait until the queue has at least 1 item
    unique_lock<mutex> lock(m);
    data_avail.wait(lock, [this] { return !q.empty(); });

    // 2. Pop the front item of the queue. The popped item is a vector<char>
    vector<char> poppedVector = q.front();
    q.pop();

    // 3. Convert the popped vector<char> into a char*, copy that into msg;
    //assert(poppedVector.size() <= static_cast<size_t>(size));
    //copy(poppedVector.begin(), poppedVector.end(), msg);
    
    if (poppedVector.size() > static_cast<size_t>(size)) {
        // Throw an exception or handle the error appropriately
        throw runtime_error("Buffer size is smaller than the size of the popped data");
    }

    // 4. Convert the popped vector<char> into a char*, copy that into msg;
    copy(poppedVector.begin(), poppedVector.end(), msg);

    // 4. Wake up threads that were waiting for pop
    slot_avail.notify_one();

    // 5. Return the vector's length to the caller so that they know how many bytes were popped
    return static_cast<int>(poppedVector.size());
}

size_t BoundedBuffer::size() {
    return q.size();
}
