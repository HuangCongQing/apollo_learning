"""
A lite version of display map tool, originally designed by /baidu/adu/decision, revised by
Fu, Xiaoxin (E-mail: fuxiaoxin@baidu.com) in July 2017.
"""

import sys
import gen.map_pb2 as map_pb2
import matplotlib.pyplot as plt

g_color = ['navy', 'c', 'cornflowerblue', 'gold', 'darkorange', 'darkviolet', 'aquamarine',
        'firebrick', 'limegreen']


def draw_line(line_segment, color):
    """
    :param line_segment:
    :return: none
    """
    px = []
    py = []
    for p in line_segment.point:
        px.append(float(p.x))
        py.append(float(p.y))
    px = downsample_array(px)
    py = downsample_array(py)
    plt.gca().plot(px, py, lw = 10, alpha=0.8, color = color)
    return px[len(px) // 2], py[len(py) // 2]


def draw_arc(arc):
    """
    :param arc: proto obj
    :return: none
    """
    xy = (arc.center.x, arc.center.y)
    start = 0
    end = 0
    if arc.start_angle < arc.end_angle:
        start = arc.start_angle / math.pi * 180
        end = arc.end_angle / math.pi * 180
    else:
        end = arc.start_angle / math.pi * 180
        start = arc.end_angle / math.pi * 180

    pac = mpatches.Arc(
        xy,
        arc.radius * 2,
        arc.radius * 2,
        angle = 0,
        theta1 = start,
        theta2 = end
    )

    plt.gca().add_patch(pac)


def downsample_array(array):
    """down sample given array"""
    skip = 5
    result = array[::skip]
    result.append(array[-1])
    return result


def draw_boundary(line_segment):
    """
    :param line_segment:
    :return:
    """
    px = []
    py = []
    for p in line_segment.point:
        px.append(float(p.x))
        py.append(float(p.y))
    px = downsample_array(px)
    py = downsample_array(py)
    plt.gca().plot(px, py, 'k')


def draw_id(x, y, id_string):
    """Draw id_string on (x, y)"""
    plt.annotate(
        id_string,
        xy = (x, y), xytext = (40, -40),
        textcoords = 'offset points', ha = 'right', va = 'bottom',
        bbox = dict(boxstyle = 'round,pad=0.5', fc = 'green', alpha = 0.5),
        arrowprops = dict(arrowstyle = '->', connectionstyle = 'arc3,rad=0'))


def get_road_index_of_lane(lane_id, road_lane_set):
    """Get road index of lane"""
    for i, lane_set in enumerate(road_lane_set):
        if lane_id in lane_set:
            return i
    return -1


def draw_map(drivemap):
    """ draw map from mapfile"""
    print 'Map info:'
    print '\tVersion:\t',
    print drivemap.header.version
    print '\tDate:\t',
    print drivemap.header.date
    print '\tDistrict:\t',
    print drivemap.header.district

    road_lane_set = []
    for road in drivemap.road:
        lanes = []
        for sec in road.section:
            for lane in sec.lane_id:
                lanes.append(lane.id)
        road_lane_set.append(lanes)

    for lane in drivemap.lane:
        #print lane.type
        #print lane.central_curve
        #break
        #print [f.name for f in lane.central_curve.DESCRIPTOR.fields]
        for curve in lane.central_curve.segment:
            if curve.HasField('line_segment'):
                road_idx = get_road_index_of_lane(lane.id.id, road_lane_set)
                if road_idx == -1:
                    print 'Failed to get road index of lane'
                    sys.exit(-1)
                center_x, center_y = draw_line(curve.line_segment,
                        g_color[road_idx % len(g_color)])
                draw_id(center_x, center_y, str(road_idx))
                #break
            if curve.HasField('arc'):
                draw_arc(curve.arc)
                #print "arc"

        for curve in lane.left_boundary.curve.segment:
            if  curve.HasField('line_segment'):
                draw_boundary(curve.line_segment)

        for curve in lane.right_boundary.curve.segment:
            if  curve.HasField('line_segment'):
                draw_boundary(curve.line_segment)
                #break

    return drivemap


def print_help():
    """Print help information.

    Print help information of usage.

    Args:

    """
    print 'usage:'
    print '     python road_show.py base_map_file_path',


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print 'Wrong number of arguments.'
        print_help()
        sys.exit(0)
    fin = open(sys.argv[1])
    base_map = map_pb2.Map()
    base_map.ParseFromString(fin.read())

    plt.subplots()
    draw_map(base_map)
    plt.axis('equal')
    plt.show()
