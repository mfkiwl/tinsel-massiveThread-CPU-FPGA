// SPDX-License-Identifier: BSD-2-Clause
#include "matrixmult.h"
#include "matrices.h"

#include <HostLink.h>
#include <POLite.h>
#include <sys/time.h>

int main() {
    
    if (!mult_possible) {
        printf("Multilpication not possible with supplied matrices!\n");
    }
    else {

        // Connection to tinsel machine
        HostLink hostLink;

        // Create POETS graph
        PGraph<MatDevice, MatState, None, MatMessage> graph;

        // Create 3D mesh of devices
        PDeviceId mesh[MESHLEN][MESHWID][MESHHEI];
        for (uint32_t x = 0; x < MESHLEN; x++) {
            for (uint32_t y = 0; y < MESHWID; y++) {
                for (uint32_t z = 0; z < MESHHEI; z++) {
                    mesh[x][y][z] = graph.newDevice();
                }
            }
        }
        
        // Add edges
        for (uint32_t x = 0; x < MESHLEN; x++) {
            for (uint32_t y = 0; y < MESHWID; y++) {
                for (uint32_t z = 0; z < MESHHEI; z++) {
                    if (x < MESHLEN-1) {
                        graph.addEdge(mesh[x][y][z], 0, mesh[x+1][y][z]);
                        }
                    if (y < MESHWID-1) {
                        graph.addEdge(mesh[x][y][z], 0, mesh[x][y+1][z]);
                        }
                    if (z < MESHHEI-1) {
                        graph.addEdge(mesh[x][y][z], 0, mesh[x][y][z+1]);
                        }
                }
            }
        } 

        // Prepare mapping from graph to hardware
        graph.map();
                    
        // Initialise device coordinates/dimensions
        for (uint32_t x = 0; x < MESHLEN; x++) {
            for (uint32_t y = 0; y < MESHWID; y++) {
                for (uint32_t z = 0; z < MESHHEI; z++) {
                    
                    //printf("X = %d: Y = %d: Z = %d ", x, y, z);
                    //printf("ID = %d ", mesh[x][y][z]);
                    //printf("\n");
                    
                    // Initialise device IDs
                    graph.devices[mesh[x][y][z]]->state.id = mesh[x][y][z];
                    
                    // Initialise Mesh coordinates on devices
                    graph.devices[mesh[x][y][z]]->state.x = x;
                    graph.devices[mesh[x][y][z]]->state.y = y;
                    graph.devices[mesh[x][y][z]]->state.z = z;

                    //Inform each device of matrix size for message passing decisions
                    graph.devices[mesh[x][y][z]]->state.xmax = MESHLEN;
                    graph.devices[mesh[x][y][z]]->state.ymax = MESHWID;
                    graph.devices[mesh[x][y][z]]->state.zmax = MESHHEI;
                }
            }
        }

        // Write graph down to tinsel machine via HostLink
        graph.write(&hostLink);

        // Load code and trigger execution
        hostLink.boot("code.v", "data.v");
        hostLink.go();
        printf("Starting\n");

        // Start timer
        struct timeval start, finish, diff;
        gettimeofday(&start, NULL);


        int deviceAddr = 0;
        
        for (uint32_t h = 0; h < MESHHEI; h++) {
            for (uint32_t w = 0; w < MESHWID; w++) {
                for (uint32_t l = 0; l < MESHLEN; l++) {
                    // Construct messages -> One same element from each matrix
                    PMessage<None, MatMessage> sendMsg;

                    printf("L:%d, W:%d, H:%d\n", l, w, h);

                    // From maxtrix A
                    deviceAddr = graph.toDeviceAddr[mesh[0][w][h]];
                    //printf("deviceAddr = %d\n", deviceAddr);
                    sendMsg.devId = getLocalDeviceId(deviceAddr);
                    sendMsg.payload.from = EXTERNALX;
                    sendMsg.payload.element1 = matrixA[w][h];
                    //printf("%d \n ", matrixA[l][w]);
                    hostLink.send(getThreadId(deviceAddr), 2, &sendMsg);
                    printf("Sent %d from matrix A to mesh[0][%d][%d]\n", sendMsg.payload.element1, w, h);

                    // From maxtrix B
                    deviceAddr = graph.toDeviceAddr[mesh[l][0][h]];
                    //printf("deviceAddr = %d\n", deviceAddr);
                    sendMsg.devId = getLocalDeviceId(deviceAddr);
                    sendMsg.payload.from = EXTERNALY;
                    sendMsg.payload.element2 = matrixB[h][l];
                    //printf("%d \n ", matrixB[w][l]);
                    hostLink.send(getThreadId(deviceAddr), 2, &sendMsg);
                    printf("Sent %d from matrix B to mesh[%d][0][%d]\n", sendMsg.payload.element2, l, h);

                }
            }
        }

        // Allocate array to contain final value of each device
        uint32_t result[MESHLEN][MESHWID] {};

        // Receive final value of each device
        for (uint32_t i = 0; i < RETMATSIZE; i++) {
            // Receive message
            PMessage<None, MatMessage> msg;
            hostLink.recvMsg(&msg, sizeof(msg));
            if (i == 0) gettimeofday(&finish, NULL);
            // Save final value
            result[graph.devices[msg.payload.from]->state.x][graph.devices[msg.payload.from]->state.y] = msg.payload.aggregate;
            
        }

        // Display time
        timersub(&finish, &start, &diff);
        double duration = (double) diff.tv_sec + (double) diff.tv_usec / 1000000.0;
        printf("Time = %lf\n", duration);

        for (uint32_t y = 0; y < MESHWID; y++) {
            for (uint32_t x = 0; x < MESHLEN; x++) {
                //printf("X = %d: Y = %d ", x, y);
                printf("%d ", result[x][y]);
            }
            printf("\n");
        }
    
    }
    
    return 0;

}
