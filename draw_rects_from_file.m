function draw_rects_from_file(rect_name, net_name)
%DRAW_RECTS_FROM_FILE �˴���ʾ�йش˺�����ժҪ
%   �˴���ʾ��ϸ˵��
rects = load(rect_name);
nets = int32(load(net_name));
draw_rects(rects, nets);
end

