function draw_rects_from_file(rect_name, net_name)
%DRAW_RECTS_FROM_FILE 此处显示有关此函数的摘要
%   此处显示详细说明
rects = load(rect_name);
nets = int32(load(net_name));
draw_rects(rects, nets);
end

