function resp=fb_comm(cmd,port)
 % fb_comm: communicate with freebird via serial comms.
    % Usage: something like fb_comm('info','COM1')
    %  cmd: 'info','datetime_read','datetime_sync'
    %   info: return cell array of system information
    %   datetime_read: accurate comparison between system clock and
    %      freebird clock
    %   datetime_sync: accurate sync of system clock to freebird.
    
    % 'accurate' is 5-50ms error.

    % port:
    %  Windows: 'COM1' or similar.
    %  Linux: most likely '/dev/ttyACM0', but
    %   refer to matlab docs on how to get Matlab to recognize ports
    %   other than /dev/ttySNNN (i.e. create a java.opts file)
    %  OSX: likely /dev/tty.usbmodemNNNNNN, where the number depends 
    %   on the specific device.  
    ser=serial(port,'BaudRate',115200);
    fopen(ser);

    try
        switch cmd
            case 'info'
                fprintf(ser,'info\n');
                resp=read_response(ser);
            case 'datetime_read'
                fprintf(ser,'datetime\n');
                resp=read_response(ser);
                sys_time=now;
                % resp{2} is like 'datetime: 2015-03-09 08:48:21'
                fb_now=datenum(resp{2}(11:end));
                fmt='yyyy-mm-dd HH:MM:SS FFF';
                fprintf('Freebird clock:  %s\n',datestr(fb_now,fmt));
                fprintf('Local(PC) clock: %s\n',datestr(now,fmt));
                delta=fb_now-now;
                fprintf('Delta: %.3fs (pos=fast)\n',delta*86400);
            case 'datetime_sync'
                % Wait until seconds rollover 
                while 1 % wait for latter half of second
                    t=clock;
                    if mod(t(6),1.0)>0.5
                        break;
                    end
                end
                while 1 % then sync to rollover
                    t=clock;
                    if mod(t(6),1.0)<0.5
                        fprintf(ser,'datetime=%s\n',datestr(t,'yyyy-mm-dd HH:MM:SS'));
                        resp=t;
                        break
                    end
                end
        end
        fclose(ser);
    catch EXC
        % opening a serial port locks it, so be careful about 
        % closing it in case there is an error while using it
        fclose(ser);
        rethrow(EXC);
    end
end

function resp=read_response(ser)
    resp={};
    while 1
        l=fscanf(ser);
        if strcmp(l,'') || ~isempty(regexp(l,'^freebird>','once'))
            break
        end
        resp{end+1} = strtrim(l);
    end
    resp=resp'; % displays in a more readable layout
end