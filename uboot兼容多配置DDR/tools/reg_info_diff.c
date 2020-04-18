#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>

#define BUFFER_MAX (10240)
#define REG_MAGIC  (0x1959)

#define OUTPUT_BIN_NAME "reg_info.bin"

struct diff_reg_unit{
	unsigned short m_index;
	unsigned short m_value;
};

struct diff_reg_head{
	unsigned short magic;
	unsigned short m_len;
};

struct diff_reg_info{
	struct diff_reg_head m_head;
	struct diff_reg_unit m_unit[1];
};

unsigned int get_file_size(char *file)
{
	struct stat file_stat;
	stat(file, &file_stat);
	return file_stat.st_size;
}

int write_diff_tail(unsigned char *base, char *add, void *add_index, unsigned short data_len)
{
	FILE *add_file = NULL;
	unsigned short i = 0;
	unsigned char *add_file_buffer = NULL;
	struct diff_reg_info *reg_info = (struct diff_reg_info *)add_index;
	int re_val = 0;
	unsigned short gpio_ctrl = atoi(add);

	if (NULL == (add_file = fopen(add, "rb")))
	{
		printf("add open fail\n");
		return -1;
	}

	if (NULL == (add_file_buffer = malloc(BUFFER_MAX)))
	{
		printf("add open fail\n");
		fclose(add_file);
		return -1;
	}

	if (gpio_ctrl > 7)
	{
		re_val = -1;
	}
	reg_info->m_head.magic = REG_MAGIC | (gpio_ctrl << 13);
	reg_info->m_head.m_len = 0;
	fread(add_file_buffer, 1, BUFFER_MAX, add_file);

	for ( i = 0; i < data_len; i++)
	{
		if (base[i] != add_file_buffer[i])
		{
			if ((void *)&reg_info->m_unit[reg_info->m_head.m_len] - (void *)base >= BUFFER_MAX)
			{
				printf("write over error\n");
				re_val = -1;
				break;
			}
			reg_info->m_unit[reg_info->m_head.m_len].m_index = i;
			reg_info->m_unit[reg_info->m_head.m_len++].m_value = add_file_buffer[i];
		}
	}
	free(add_file_buffer);
	fclose(add_file);
	return (re_val == -1) ? (-1) : (reg_info->m_head.m_len);
}

int main(int argc, char **argv)
{
	FILE *base_bin = NULL;
	DIR * src_dir = NULL;
	struct dirent * cur_bin = NULL;
	unsigned int bin_size = 0;
	void *base_bin_buffer = NULL;
	int write_len = 0;
	unsigned short data_len = 0;

	if (NULL == (base_bin = fopen(argv[1], "rb")))
	{
		printf("open base_bin fail\n");
		goto EXIT1;
	}

	if (NULL == (src_dir = opendir(argv[2])))
	{
		printf("open dir error\n");
		goto EXIT2;
	}

	if (NULL == (base_bin_buffer = malloc(BUFFER_MAX)))
	{
		printf("malloc error\n");
		goto EXIT3;
	}

	if (BUFFER_MAX <= (bin_size = get_file_size(argv[1])))
	{
		printf("base bin is too big\n");
		goto EXIT4;
	}

	memset(base_bin_buffer, 0x00, BUFFER_MAX);
	if (0 > (bin_size = fread(base_bin_buffer, 1, BUFFER_MAX, base_bin)))
	{
		printf("read bin error\n");
		goto EXIT4;
	}
	data_len = bin_size;

	while(cur_bin = readdir(src_dir))
	{
		
		if (NULL == strstr(cur_bin->d_name, ".bin"))
		{
			continue;
		}

		if (0 == strncmp(argv[1], cur_bin->d_name, strlen(argv[1])))
		{
			continue;
		}

		if (0 == strncmp(OUTPUT_BIN_NAME, cur_bin->d_name, strlen(argv[1])))
		{
			continue;
		}

		printf("add:%s\n",cur_bin->d_name);
		if (0 > (write_len = write_diff_tail(base_bin_buffer, cur_bin->d_name, base_bin_buffer + bin_size, data_len)))
		{
			printf("write taill error\n");
			goto EXIT4;
		}
		bin_size += (write_len * sizeof(struct diff_reg_unit)) + 4;
	}

	remove(OUTPUT_BIN_NAME);
	FILE *new_reg = fopen(OUTPUT_BIN_NAME,"ab");
	if (BUFFER_MAX != fwrite(base_bin_buffer, 1, BUFFER_MAX, new_reg))
	{
		printf("write new bin error\n");
	}
	fclose(new_reg);
	printf("Done\n");
EXIT4:
	free(base_bin_buffer);
EXIT3:
	closedir(src_dir);
EXIT2:
	fclose(base_bin);
EXIT1:
	return 0;
}