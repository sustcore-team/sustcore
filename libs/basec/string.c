/**
 * @file string.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief string.h函数实现
 * @version alpha-1.0.0
 * @date 2025-11-17
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <string.h>

void memset(void *restrict dst, int val, size_t size) {
	// 逐字节设置
	// 相信编译器, 它一定能优化好的(吗?)
	for (int i = 0 ; i < size ; i ++) {
		*(char *)(dst + i) = val & 0xFF;
	}
}

void memcpy(void *restrict dst, const void *restrict src, size_t size) {
	// 逐字节复制
	// 相信编译器, 它一定能优化好的(吗?)
	for (int i = 0 ; i < size ; i ++) {
		*(char *)(dst + i) = *(char *)(src + i);
	}
}

// 比较字符串
int strcmp(const char *restrict str1, const char *restrict str2) {
	int ret = 0;
	// 挨个匹配
	do {
		ret = *(str1) - *(str2);
		// 移动指针
		str1 ++;
		str2 ++;
	} while (*str1 != 0 && *str2 != 0 && ret == 0);

	// *str2 = 0而*str1 != 0 => len(str1) > len(str2) => str1 > str2
	if (*str1 != 0) return 1;
	else if (*str2 != 0) return -1;

	// 根据ret符号获得比较结果
	if (ret < 0) return -1;
	else if (ret > 0) return 1;
	// 相等
	return 0;
}

//计算str长度
int strlen(const char *restrict str) {
	int len = 0;
	while (*str != 0) {
		len += 1;
		str ++;
	}
	return len;
}

int strnlen(const char *restrict str, int count) {
	int len = 0;
	while (*str != 0 && len < count) {
		len += 1;
		str ++;
	}
	return len;
}

// 复制字符串
char *strcpy(void *restrict dst, const char *restrict src) {
	int len = strlen(src);
	memcpy(dst, src, len);
	return dst;
}

// 查找字符
char *strchr(const char *restrict str, char ch) {
	// 挨个查找
	while (*str != 0) {
		if (*str == ch) {
			return (char *)str;
		}
	}
	// 未查找到对应字符
	return NULL;
}

// 匹配字符串(前count个字符)
int strncmp(const char *restrict str1, const char *restrict str2, int count){
	int ret = 0;
	// 挨个匹配
	do {
		ret = *(str1) - *(str2);
		// 移动指针
		str1 ++;
		str2 ++;
		count --;
	} while (*str1 != 0 && *str2 != 0 && ret == 0 && count > 0);
	// 根据ret符号获得比较结果
	if (ret < 0) return -1;
	else if (ret > 0) return 1;
	// 相等
	return 0;
}

// 复制字符串(前count个字符)
char *strncpy(char *restrict dst, const char *restrict src, int count) {
	char *_dst = (char *)dst;
	// 挨个复制
	while (*src != 0 && count > 0) {
		*_dst = *src;
		// 移动指针
		_dst ++;
		src ++;
		count --;
	}
	return dst;
}

// 查找字符(末次出现)
char *strrchr(const char *restrict str, char ch){
	char *ret = NULL;
	// 挨个查找
	while (*str != 0) {
		if (*str == ch) {
			// 覆盖上次的结果
			ret = (char *)str;
		}
	}
	return ret;
}

// 返回str从开头开始连续在accept中出现过的字符的个数
// 时间复杂度O(S+A)
int strspn(const char *restrict str, const char *restrict accept) {
	// 记录accept中的字符
	char map[128] = {};
	while ((int)*accept != 0) {
		//将accept中出现过的字符在map中标记
		map[(int)*accept] = 1;
		accept ++;
	}
	int cnt = 0;
	while (*str != 0) {
		// 查表
		if (! map[(int)*str]) {
			break;
		}
		cnt ++;
	}
	return cnt;
}

// 返回str从开头开始连续没在accept中出现过的字符的个数
// 时间复杂度O(S+A)
int strcspn(const char *restrict str, const char *restrict accept) {
	// 记录accept中的字符
	char map[128] = {};
	while ((int)*accept != 0) {
		//将accept中出现过的字符在map中标记
		map[(int)*accept] = 1;
		accept ++;
	}
	int cnt = 0;
	while (*str != 0) {
		// 查表
		if (map[(int)*str]) {
			break;
		}
		cnt ++;
	}
	return cnt;
}

// 返回str在accept中出现过的第一个字符的指针
char *strpbrk(const char *restrict str, const char *restrict accept) {
	return ((char *)str) + strcspn(str, accept);
}

// 将src拼接在dst上
char *strcat(char *restrict dst, const char *restrict src) {
	// 获取dst结尾指针
	char *cat_pos = dst + strlen(dst);
	// 挨个复制
	while (*src != 0) {
		*cat_pos = *src;
		// 移动指针
		cat_pos ++;
		src ++;
	}
	return dst;
}

// 将src的前count个字符拼接在dst上
char *strncat(char *restrict dst, const char *restrict src, int count) {
	// 获取dst结尾指针
	char *cat_pos = dst + strlen(dst);
	// 挨个复制
	while (*src != 0 && count > 0) {
		*cat_pos = *src;
		// 移动指针
		cat_pos ++;
		src ++;
		count --;
	}
	return dst;
}

// // 分割字符串
// // TMD这玩意谁tmd爱写谁tmd写去
// char *strtok(char *restrict str, const char *restrict split) {
// 	//TODO: 摆烂啦哈哈哈哈不写啦
// 	return NULL;
// }

// 从src中复制size个字节到dst中
// 注意, dst与src内存区域可能重叠 不能施加restrict
void *memmove(void *dst, const void *src, size_t size) {
	// 判断是正向复制还是反向复制
	if (dst < src) {
		// 正向复制
		for (size_t i = 0 ; i < size ; i ++) {
			*(char *)(dst + i) = *(char *)(src + i);
		}
	} else if (dst > src) {
		// 反向复制
		for (size_t i = size ; i > 0 ; i --) {
			*(char *)(dst + i - 1) = *(char *)(src + i - 1);
		}
	}
	return dst;
}

// 比较内存中前count个字节
int memcmp(const void *restrict str1, const void *restrict str2, int count) {
	int ret = 0;
	const char *_str1 = str1, *_str2 = str2;
	// 挨个比较
	do {
		ret = *(_str1) - *(_str2);
		// 移动指针
		str1 ++;
		str2 ++;
		count --;
	} while (ret == 0 && count > 0);
	// 根据ret符号获得比较结果
	if (ret < 0) return -1;
	else if (ret > 0) return 1;
	// 相等
	return 0;
}

// 在内存的前count个字节中查找ch
void *memchr(const void *restrict str, char ch, int count) {
	char *_str = (char *)str;
	// 挨个查找
	while (count > 0) {
		if (*_str == ch) {
			return _str;
		}
		_str ++;
		count --;
	}
	// 未查找到
	return NULL;
}