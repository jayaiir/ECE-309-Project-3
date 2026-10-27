#pragma once
#include <string>
#include <cstddef>
#include <stdexcept>
#include "message.h"

class Conversation {
public:

    Conversation() = default; // Default constructor used to create an empty conversation
    
    std::size_t size() const noexcept { return size_; } // return the number of messages in the conversation
    const Message* begin() const noexcept { return data_; }        
    const Message* end()   const noexcept { return data_ + size_; } // address one past the last message

    // Access message at a specific index and throw if out of range
    const Message& at(std::size_t i) const {
        if (i >= size_) {
            throw std::out_of_range("Conversation::at: index out of range");
        }
        return data_[i];
    } 

    void append(Message m){
        if (size_ == capacity_) {
            std::size_t new_capacity = (capacity_ == 0) ? 1 : capacity_ * 2; // double the capacity starting from 1 if currently empty
            Message* new_data = new Message[new_capacity]; // allocate new memory for the messages
            for (std::size_t i = 0; i < size_; ++i) { // move existing messages to the new array
                new_data[i] = std::move(data_[i]);
            }
            delete[] data_;
            data_ = new_data;
            capacity_ = new_capacity;
        }
        data_[size_++] = std::move(m);
    } // append a message to the conversation

    //RULE OF FIVE: Destructor, Copy Constructor, Copy Assignment Operator, Move Constructor, Move Assignment Operator

    // Destructor to free the allocated memory for the messages
    ~Conversation() {
        delete[] data_;
    }

    // Copy constructor 
    Conversation(const Conversation& other)
    : size_(other.size_), capacity_(other.capacity_) {
    if (capacity_ > 0) {
        data_ = new Message[capacity_];
        for (std::size_t i = 0; i < size_; ++i) {
            data_[i] = other.data_[i];
            }
        }
    } 

    // Copy assignment operator=
    Conversation& operator=(const Conversation& other) {
    if (this != &other) {
        Message* new_data = nullptr;
        if (other.capacity_ > 0) {
            new_data = new Message[other.capacity_];  
            for (std::size_t i = 0; i < other.size_; ++i) {
                new_data[i] = other.data_[i];
                }
            }
        delete[] data_;
        data_ = new_data;
        size_ = other.size_;
        capacity_ = other.capacity_;
        }
        return *this;
    }

    // Move constructor
    Conversation(Conversation&& other) noexcept {
        size_ = other.size_;
        capacity_ = other.capacity_;
        data_ = other.data_;
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }

    // Move assignment operator
    Conversation& operator=(Conversation&& other) noexcept {
        if (this != &other) {
            delete[] data_;
            size_ = other.size_;
            capacity_ = other.capacity_;
            data_ = other.data_;
            other.data_ = nullptr;
            other.size_ = 0;
            other.capacity_ = 0;
        }
        return *this;
    }

    std::size_t capacity() const noexcept { return capacity_; } // for tests

private:
    std::size_t size_ = 0; 
    std::size_t capacity_ = 0; 
    Message* data_ = nullptr; 

};