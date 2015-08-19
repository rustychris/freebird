function [ fb ] = fb_parse( fn, keep_raw )
%fb_parse: parse raw binary data from freebird logger
%   Returns matlab struct with config metadata and parsed fields
%   optional second parameter, if nonzero, will include low level
%   data.

if nargin == 1
    keep_raw=0;
end

fb_common;

file_info=dir(fn);
if exist(fn) ~= 2
    error('%s not found',fn);
end

file_nbytes=file_info.bytes;
% arduino/teensy writes little-endian data
fid=fopen(fn,'rb','ieee-le');

clear fb;
fb.file_nbytes=file_nbytes;

fb.nblocks=fb.file_nbytes/block_nbytes;
if fb.nblocks ~= floor(fb.nblocks)
    error('File %s is corrupt: not an integer number of blocks',fn);
end

fb.config_txt='';
bytes_per_frame=0;     % gets set after reading the initial text blocks
frames_per_block=0; % ditto

% establish the types for the block structure
% metadata from each block is saved into a struct array.
% raw data is put into a 2D array
% text data is put into a 1D array.
% each block has an index into the respective array
dummy.unixtime=cast(0,'uint32');
dummy.ticks=cast(0,'uint16');
dummy.frame_count=cast(0,'uint8');
dummy.flags=cast(0,'uint8');
dummy.type=cast(0,'uint8');
% 
dummy.text_idx=cast(0,'uint32');
dummy.data_idx=cast(0,'uint32');

raw_data=[]; % will be preallocated when the size is known.

fb.blocks(fb.nblocks)=dummy; % preallocated struct array for metadata

% basic profiling shows that half the time is in fread calls.
% profile time was about 270s 
% now, should have just one fread per block, down to 190s.

for blk_id=1:fb.nblocks
    if mod(blk_id,1000)==0
        fprintf('Block %d, %.2f%%\n',blk_id,100*blk_id/fb.nblocks);
    end
    
    % seek unnecessary if scanning entire file
    % re-enabled during debugging
    fseek(fid,block_nbytes*(blk_id-1),'bof');
    
    blk=fb.blocks(blk_id);
    hdr_bytes=8;
    
    buff=fread(fid,block_nbytes,'*uint8');
    blk.unixtime=typecast(buff(1:4),'uint32');
    blk.ticks=typecast(buff(5:6),'uint16');
    blk.frame_count=buff(7);
    blk.flags=buff(8);
    raw_block=buff(hdr_bytes+1:end);
    blk.type=(blk.flags&FLAG_TYPE_MASK);

    % BUG: every 5 frames, we're losing a byte it appears.
    % probably have a transpose issue
    if blk.type == FLAG_TYPE_DATA
        if bytes_per_frame==0
            fb=parse_header_text(fb);
            bytes_per_frame=fb.config.frame_bytes;
            frames_per_block=floor((block_nbytes-hdr_bytes)/bytes_per_frame);
            % preallocated raw array
            raw_data=zeros([frames_per_block*fb.nblocks,bytes_per_frame],'uint8');
            raw_idx=1; % running index of next available row.
        end
        %raw_block=fread(fid,[frames_per_block,bytes_per_frame],'*uint8');
        raw_block=raw_block(1:(frames_per_block*bytes_per_frame));
        % matlab ordering is opposite python
        raw_block=reshape(raw_block,[bytes_per_frame,frames_per_block])';
        
        raw_data(raw_idx:raw_idx+frames_per_block-1,:)=raw_block;
        blk.data_idx=raw_idx;
        raw_idx=raw_idx+frames_per_block;
    else
        % txt=fread(fid,block_nbytes-hdr_bytes,'*char');
        txt=raw_block';
        null=strfind(txt,0);
        if null
            txt=txt(1:null-1);
        end
        blk.text_idx=1+length(fb.config_txt);
        fb.config_txt=[fb.config_txt txt];
    end
    fb.blocks(blk_id)=blk;
end
fclose(fid);

fb.raw_data=raw_data(1:(raw_idx-1),:); % trim, as prealloc is conservatively large
fb=frames_to_fields(fb);
fb=add_timeline(fb);

fb.volts=cast(fb.counts,'double') * 4.096/32768;

if ~keep_raw
    fb=rmfield(fb,'config_txt');
    fb=rmfield(fb,'raw_data');
end

end

function [fb]=parse_header_text(fb)
  % strsplit only in newer matlab
  % lines=strsplit(fb.config_txt,'\n');
  % use regexp instead:
  lines=regexp(fb.config_txt,'\n','split');
  fb.config=[];
  for line=lines
      trimmed=strtrim(line{1});
      split_pos=strfind(trimmed,':');
      if isempty(split_pos)
          continue
      end
      key=strtrim(trimmed(1:split_pos(1)-1));
      value=strtrim(trimmed(split_pos(1)+1:end));
      num_value=str2double(value);
      if ~isnan(num_value)
          value=num_value;
      end
      fb.config.(key)=value;
  end
end

function [fb]=frames_to_fields(fb)
  % once all the binary data has been read out, this
  % separates the frames into fields
  % format is specified in numpy dtype syntax, 
  % fb.config.frame_format something like
  %  [('counts','<i2'),('imu_a','<i2',3),('imu_g','<i2',3),('imu_m','<i2',3),]  
  
  % initial test and the counts field is corrupt.
  % unixtime and ticks appear fine.
  % text blocks look good.
  
  m=regexp(fb.config.frame_format,'\(''([a-zA-Z_1-9]*)'',([^\)]*)','tokens');
  byte_off=1;
  nframes=length(fb.raw_data);
  
  % cheesy test for native endianness
  if typecast(cast([1 0],'uint8'),'uint16')==1
      arch='le';
  else
      warning('Big-endian architecture not tested.');
      arch='be';
  end
  
  for fld_idx=1:length(m)
    fld_def=m{fld_idx};
    name=fld_def{1};
    fmt=fld_def{2}; % ala '<i2' or '<i2',3
    
    % use regexp for backwards compatibility before R2013a
    %parts=strsplit(fmt,',');
    parts=regexp(fmt,',','split');
    
    item=parts{1};
    item=item(2:end-1);
    if strcmp(item,'<i2')
        itembytes=2;
        itemcode='int16';
    else
        error('What is field format %s?',item);
    end
  
    if length(parts)>1
        dim=str2double(parts(2));
    else
        dim=1;
    end
    final_shape=[dim nframes];
    
    raw_fld=fb.raw_data(:,byte_off:byte_off+itembytes*dim-1);
    % transpose to get order LSB,MSB,LSB,MSB
    raw_fld=reshape(raw_fld',[],1);
    item_fld=typecast(raw_fld,itemcode);
    if strcmp(arch,'be')
        item_fld=swabytes(item_fld);
    end
    % put sample as first index, vector dimension second
    fb.(name)=reshape(item_fld,final_shape)';
    
    byte_off=byte_off+dim*itembytes;
  end
end

function [fb]=add_timeline(fb)
    fb_common;
    % convert block timestamps to datenums:
    unixsecs=cast( cell2mat({fb.blocks.unixtime}), 'double');
    ticks   =cast( cell2mat({fb.blocks.ticks}), 'double');
    unixtime=unixsecs + ticks/fb.config.ticks_per_second;
    unix0=datenum('1970', 'yyyy');
    block_dn=unix0+unixtime/86400;
    data_blocks=( cell2mat({fb.blocks.type})==FLAG_TYPE_DATA );
    block_dn=block_dn(data_blocks);
    block_idx=cell2mat({fb.blocks(data_blocks).data_idx});
    % if there were overruns, it would be more correct to use the sample
    % rate to fill in.
    fb.dn=interp1(block_idx,block_dn,1:length(fb.raw_data) );
    % still have to fill in the last frame:
    dt=1/fb.config.sample_rate_hz / 86400;
    A=block_idx(end);
    B=length(fb.dn);
    fb.dn(A+1:B) = fb.dn(A) + dt*( 1:(B-A) );
    fb.dn=fb.dn'; % consistent with other fields.

end
