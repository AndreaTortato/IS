#include "indexBuffer.h"
#include <GL/glew.h>

IndexBuffer::IndexBuffer() {
	//generate a new OpenGL buffer
	glGenBuffers(1, &mHandle);
	mCount = 0;
}
IndexBuffer::~IndexBuffer() {
	glDeleteBuffers(1, &mHandle);
}

//getters
unsigned int IndexBuffer::Count() {
	return mCount;
}
unsigned int IndexBuffer::GetHandle() {
	return mHandle;
}

//setters
void IndexBuffer::Set(unsigned int* inputArray, unsigned
	int arrayLengt) {
	mCount = arrayLengt;
	unsigned int size = sizeof(unsigned int);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mHandle);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, size * mCount,
		inputArray, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void IndexBuffer::Set(std::vector<unsigned int>& input) {
	Set(&input[0], (unsigned int)input.size());
}