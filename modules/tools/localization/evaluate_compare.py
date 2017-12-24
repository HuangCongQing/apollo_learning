#!/usr/bin/python

import sys 
import numpy
import math
import os

def get_stat2_from_data(data):
    """find the max number of continuous frames when position error is lager than 30cm, 20cm and 10cm
    Args:
        data: error array 
    Returns:
        stat: array of max number of continuous frames
    """
    max_con_frame_num_10 = 0
    max_con_frame_num_20 = 0
    max_con_frame_num_30 = 0

    tem_con_frame_num_10 = 0
    tem_con_frame_num_20 = 0
    tem_con_frame_num_30 = 0

    for d in data:
        if d > 0.1:
            tem_con_frame_num_10 += 1
            if d > 0.2:
                tem_con_frame_num_20 += 1
                if d > 0.3:
                    tem_con_frame_num_30 += 1
                else:
                    if tem_con_frame_num_30 > max_con_frame_num_30:
                        max_con_frame_num_30 = tem_con_frame_num_30  
                        tem_con_frame_num_30 = 0
            else:
                if tem_con_frame_num_20 > max_con_frame_num_20:
                    max_con_frame_num_20 = tem_con_frame_num_20  
                    tem_con_frame_num_20 = 0
        else: 
            if tem_con_frame_num_10 > max_con_frame_num_10:
                max_con_frame_num_10 = tem_con_frame_num_10  
                tem_con_frame_num_10 = 0

    stat = [max_con_frame_num_10, max_con_frame_num_20, max_con_frame_num_30]
    return stat        


def get_angle_stat2_from_data(data):
    """find the max number of continuous frames when yaw error is lager than 1.0d, 0.6d and 0.3d
    Args:
        data: error array 
    Returns:
        stat: array of max number of continuous frames
    """
    max_con_frame_num_1_0 = 0
    max_con_frame_num_0_6 = 0
    max_con_frame_num_0_3 = 0

    tem_con_frame_num_1_0 = 0
    tem_con_frame_num_0_6 = 0
    tem_con_frame_num_0_3 = 0

    for d in data:
        if(d > 0.3):
            tem_con_frame_num_0_3 += 1
            if(d > 0.6):
                tem_con_frame_num_0_6 += 1
                if(d > 1.0):
                    tem_con_frame_num_1_0 += 1
                else:
                    if tem_con_frame_num_1_0 > max_con_frame_num_1_0:
                        max_con_frame_num_1_0 = tem_con_frame_num_1_0  
                        tem_con_frame_num_1_0 = 0
            else:
                if tem_con_frame_num_0_6 > max_con_frame_num_0_6:
                    max_con_frame_num_0_6 = tem_con_frame_num_0_6  
                    tem_con_frame_num_0_6 = 0
        else:
            if tem_con_frame_num_0_3 > max_con_frame_num_0_3:
                max_con_frame_num_0_3 = tem_con_frame_num_0_3  
                tem_con_frame_num_0_3 = 0

    stat = [max_con_frame_num_1_0, max_con_frame_num_0_6, max_con_frame_num_0_3]
    return stat


def get_stat_from_data(data):
    num_data = numpy.array(data)
    sum1 = num_data.sum()
    sum2 = (num_data * num_data).sum()
    mean = sum1 / len(data)
    std = math.sqrt(sum2 / len(data) - mean * mean)
    mx = num_data.max()
    count_less_than_30 = 0.0
    count_less_than_20 = 0.0
    count_less_than_10 = 0.0
    for d in data:
        if d < 0.3:
            count_less_than_30 += 1.0
            if d < 0.2:
                count_less_than_20 += 1.0
                if d < 0.1:
                    count_less_than_10 += 1.0
    count_less_than_30 /= float(len(data))
    count_less_than_20 /= float(len(data))
    count_less_than_10 /= float(len(data))
    stat = [mean, std, mx, count_less_than_30, count_less_than_20, count_less_than_10]
    return stat


def get_angle_stat_from_data(data):
    num_data = numpy.array(data)
    sum1 = num_data.sum()
    sum2 = (num_data * num_data).sum()
    mean = sum1 / len(data)
    std = math.sqrt(sum2 / len(data) - mean * mean)
    mx = num_data.max()
    count_less_than_1 = 0.0
    count_less_than_06 = 0.0
    count_less_than_03 = 0.0
    for d in data:
        if d < 1.0:
            count_less_than_1 += 1.0
            if d < 0.6:
                count_less_than_06 += 1.0
                if d < 0.3:
                    count_less_than_03 += 1.0
    count_less_than_1 /= float(len(data))
    count_less_than_06 /= float(len(data))
    count_less_than_03 /= float(len(data))
    stat = [mean, std, mx, count_less_than_1, count_less_than_06, count_less_than_03]
    return stat


def parse_file(filename):
    file = open(filename, "r")
    lines = file.readlines()
    print "%d frames" % len(lines)
    error = []
    error_lon = []
    error_lat = []
    error_alt = []
    error_roll = []
    error_pitch = []
    error_yaw = []
    for line in lines:
        s = line.split()
        if (len(s) > 7):
            #error.append(float(s[6]))
            error_lon.append(float(s[2]))
            error_lat.append(float(s[3]))
            error_alt.append(float(s[4]))
            error_roll.append(float(s[5]))
            error_pitch.append(float(s[6]))
            error_yaw.append(float(s[7]))
            
            x = float(s[2])
            y = float(s[3])
            error.append(math.sqrt(x * x + y * y))
            #print "%f %f %f" % (error[-1], error_lon[-1], error_lat[-1])
    file.close()
    
    print "criteria : mean     std      max      < 30cm   < 20cm   < 10cm  con_frames(>30cm)"
    result = get_stat_from_data(error)
    res = get_stat2_from_data(error)
    print "error    : %06f %06f %06f %06f %06f %06f %06d" % \
    (result[0], result[1], result[2], result[3], result[4], result[5], res[2]) 
    result = get_stat_from_data(error_lon)
    res = get_stat2_from_data(error_lon)
    print "error lon: %06f %06f %06f %06f %06f %06f %06d" % \
    (result[0], result[1], result[2], result[3], result[4], result[5], res[2]) 
    result = get_stat_from_data(error_lat)
    res = get_stat2_from_data(error_lat)
    print "error lat: %06f %06f %06f %06f %06f %06f %06d" % \
    (result[0], result[1], result[2], result[3], result[4], result[5], res[2]) 
    result = get_stat_from_data(error_alt)
    res = get_stat2_from_data(error_alt)
    print "error alt: %06f %06f %06f %06f %06f %06f %06d" % \
    (result[0], result[1], result[2], result[3], result[4], result[5], res[2]) 

    print "criteria : mean     std      max      < 1.0d   < 0.6d   < 0.3d  con_frames(>1.0d)"
    result = get_angle_stat_from_data(error_roll)
    res = get_angle_stat2_from_data(error_roll)
    print "error rol: %06f %06f %06f %06f %06f %06f %06d" % \
    (result[0], result[1], result[2], result[3], result[4], result[5], res[0]) 
    result = get_angle_stat_from_data(error_pitch)
    res = get_angle_stat2_from_data(error_pitch)
    print "error pit: %06f %06f %06f %06f %06f %06f %06d" % \
    (result[0], result[1], result[2], result[3], result[4], result[5], res[0]) 
    result = get_angle_stat_from_data(error_yaw)
    res = get_angle_stat2_from_data(error_yaw)
    print "error yaw: %06f %06f %06f %06f %06f %06f %06d" % \
    (result[0], result[1], result[2], result[3], result[4], result[5], res[0]) 
    

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print "python evaluation.py [evaluation file]"
    elif not os.path.isfile(sys.argv[1]):
        print "file not exist"
    else:
        parse_file(sys.argv[1])

