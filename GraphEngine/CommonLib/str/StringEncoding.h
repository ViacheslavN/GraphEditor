#pragma once

namespace CommonLib
{
	namespace StringEncoding
	{
					   
		void str_utf82w(const astr& utf8, wstr& unicode);
		void str_w2utf8(const wstr& unicode, astr& utf8);
		astr str_w2utf8(const wstr& unicode);

		void str_utf82w_safe(const astr& utf8, wstr& unicode);
		void str_w2utf8_safe(const wstr& unicode, astr& utf8);

		wstr str_utf82w_safe(const astr& utf8);
		astr str_w2utf8_safe(const wstr& unicode);


		void str_w2a(const wstr& src_str, astr& stra);
		astr str_w2a(const wstr& unicode);
		astr str_w2a_safe(const wstr& unicode);
		void str_a2w(const astr& src_str, wstr& unicode);
		wstr str_a2w(const astr& src_str);
		wstr str_a2w_safe(const astr& src_str);

		void str_univ_utf82w(const astr& srca, wstr& unicode);
		void str_univ_w2utf8(const wstr& unicode, astr& stra);

		wstr str_univ_utf82w(const astr& stra);
		astr str_univ_w2utf8(const wstr& unicode);

		wstr str_univ_utf82w_safe(const astr& stra);
		astr str_univ_w2utf8_safe(const wstr& unicode);


		astr str_a2utf8(const astr& src_str);
		astr str_a2utf8_safe(const astr& src_str);

	}

}