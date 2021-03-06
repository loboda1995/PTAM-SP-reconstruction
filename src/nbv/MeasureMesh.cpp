#include "MeasureMesh.h"

MeasureMesh::MeasureMesh(std::vector<MeasureMesh::Vertex> vertices)
        : vertices_(std::move(vertices)) {

    // Create buffers
    glGenVertexArrays(1, &VAO_);
    glGenBuffers(1, &VBO_);

    // Load data
    glBindVertexArray(VAO_);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_);
    glBufferData(GL_ARRAY_BUFFER, vertices_.size() * sizeof(Vertex), vertices_.data(), GL_STATIC_DRAW);

    // Set vertex attribute pointers
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*) 0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*) offsetof(Vertex, measure));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

MeasureMesh::~MeasureMesh() {
    glDeleteVertexArrays(1, &VAO_);
    glDeleteBuffers(1, &VBO_);
}

const std::vector<MeasureMesh::Vertex>& MeasureMesh::getVertices() {
    return vertices_;
}

void MeasureMesh::draw() {
    glBindVertexArray(VAO_);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices_.size()));
    glBindVertexArray(0);
}
