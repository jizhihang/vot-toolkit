function draw_region(region, color, width)

if nargin < 2
    color = [1, 0, 0];
end;

if nargin < 3
    width = 1;
end;

if isnumeric(region) 
	if numel(region) == 4

        for i = 1:size(region, 1)
            x = [region(i, 1), region(i, 1), region(i, 1) + region(i, 3), ...
                 region(i, 1) + region(i, 3), region(i, 1)];
            y = [region(i, 2), region(i, 2) + region(i, 4), region(i, 2) + ...
                 region(i, 4), region(i, 2), region(i, 2)];

            plot(x, y, 'Color', color, 'LineWidth', width);
        end;

    elseif numel(region) >= 6 && mod(numel(region), 2) == 0

        x = region(1:2:end);
        y = region(2:2:end);        
        
        plotc(x, y, 'Color', color, 'LineWidth', width);
	end;
        
end;
