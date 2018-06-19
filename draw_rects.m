function draw_rects(rects, nets)
%DRAW_RECTS 此处显示有关此函数的摘要
%   此处显示详细说明
nets = int32(nets);
x = rects(:, 1);
y = rects(:, 2);
w = rects(:, 3) - x;
h = rects(:, 4) - y;
xmid = x + w / 2;
ymid = y + h / 2;
figure;
for i = 1:length(x)
    rectangle('position', [x(i), y(i), w(i), h(i)], 'facecolor', 'blue')
end
for i = 1: size(nets, 1)
    idx0 = nets(i, 1) + 1;
    idx1 = nets(i, 2) + 1;
    line([xmid(idx0), xmid(idx1)], [ymid(idx0) ymid(idx1)], 'color', ...
       'red'); 
end
end

