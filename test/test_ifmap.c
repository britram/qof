//
//  test_ifmap.c
//  qof
//
//  Created by Brian Trammell on 1/18/13.
//  Copyright (c) 2013 Brian Trammell. All rights reserved.
//

#define _YAF_SOURCE_
#include <yaf/autoinc.h>
#include <yaf/qofifmap.h>

#include <arpa/inet.h>

int main (int argc, char* argv[])
{
    qfIfMap_t map;

    uint8_t addr6[16];

    // create a map from scratch
    qfIfMapInit(&map);
    
    qfIfMapAddIPv4Mapping(&map,0x0A000100,24,1,11);
    qfIfMapAddIPv4Mapping(&map,0x0A000200,24,2,12);
    qfIfMapAddIPv4Mapping(&map,0x0A000300,24,3,13);
    qfIfMapAddIPv4Mapping(&map,0x0A000400,24,4,14);
    qfIfMapAddIPv4Mapping(&map,0x7f000000,8,9,19);
    
    inet_pton(AF_INET6, "2003:db8:1:2::0", addr6);
    qfIfMapAddIPv6Mapping(&map, addr6, 64, 2, 12);
    
    // FIXME run a few tests
}