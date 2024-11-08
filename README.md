# Branch Prediction Simulator
Author: Tejashree Kulkarni

University: North Carolina State University; 
Professor: Dr. Eric Rotenberg
Course: ECE 563 - Architecture of Microprocessor; 
Duration: Aug 2022- December2022

The technology used: OOPs, G-share, Bimodal, Hybrid branch algorithms

Project Accomplishment:

This repository contains a Branch Predictor Simulator. The simulator models branch prediction mechanisms commonly used in computer architecture and evaluates their performance across various configurations and trace files.

# Project Overview

The Branch Predictor Simulator aims to model and analyze the following types of branch predictors:

Bimodal Predictor: A simple predictor using only the branch's program counter (PC) for indexing.

Gshare Predictor: A global-history-based predictor that combines the branch's PC with global branch history.

Hybrid Predictor: A selector mechanism between the bimodal and gshare predictors, based on a chooser table.

Each predictor configuration is tested for its misprediction rate to assess its effectiveness in minimizing branch mispredictions.

# Features

Bimodal Predictor: Uses low-order bits of the PC to form an index into a table of 2-bit counters, predicting the outcome of branches.

Gshare Predictor: Utilizes an XOR of the global history register with the upper bits of the PC for prediction, allowing for history-based predictions.

Hybrid Predictor (ECE 563 only): Chooses between bimodal and gshare predictions based on the outcomes of a chooser table.

The simulator processes branch traces and outputs metrics to evaluate each predictor’s performance, including the total number of predictions, number of mispredictions, and the misprediction rate.

Requirements

Programming Languages: C, C++
Build Tools: Makefile

# File Structure
src/sim_bp.c, src/sim_bp.h: Source code for the simulator.
Makefile: Build configuration.
README.md: Project documentation.
report.pdf: Project report (included in final submission). 

# How to Execute

1. Type "make" to build.  (Type "make clean" first if you already compiled and want to recompile from scratch.)
 
Run trace reader:
 
To run without throttling output:
./sim bimodal 6 gcc_trace.txt
./sim gshare 9 3 gcc_trace.txt
./sim hybrid 8 14 10 5 gcc_trace.txt

To run with throttling (via "less"):
./sim bimodal 6 gcc_trace.txt | less
./sim gshare 9 3 gcc_trace.txt | less
./sim hybrid 8 14 10 5 gcc_trace.txt | less


