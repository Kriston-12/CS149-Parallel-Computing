# **Stanford CS149 - Parallel Computing (Personal Notes)**

This is a repository that records my understanding of **Stanford CS149: Parallel Computing**.

> **Note**: I am not a Stanford student, and I am using **WSL2 Ubuntu 22.04** as the environment for setup and development.

---

## **Assignment 1 - ISPC Installation Guide**

Follow the steps below to set up the environment and install the required tools for **Assignment 1**.

### **1. Install ISPC Compiler**
Run the following commands to download, extract, and install the **ISPC (Intel SPMD Program Compiler)**:

```bash
wget https://github.com/ispc/ispc/releases/download/v1.23.0/ispc-v1.23.0-linux.tar.gz
tar -xvf ispc-v1.23.0-linux.tar.gz
sudo mv ispc-v1.23.0-linux/bin/ispc /usr/local/bin
rm -rf ispc-v1.23.0-linux.tar.gz ispc-v1.23.0-linux
