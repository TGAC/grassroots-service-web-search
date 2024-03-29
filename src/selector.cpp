/*
** Copyright 2014-2016 The Earlham Institute
** 
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
** 
**     http://www.apache.org/licenses/LICENSE-2.0
** 
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/
/*
 * selector.cpp
 *
 *  Created on: 12 Feb 2015
 *      Author: tyrrells
 */

#include "selector.hpp"

#include <string>
#include <iostream>

#include <htmlcxx/html/ParserDom.h>

#include "hcxselect.h"

#include "streams.h"
#include "memory_allocations.h"
#include "string_utils.h"
#include "byte_buffer.h"
#include "data_resource.h"

using namespace std;
using namespace htmlcxx :: HTML;
//using namespace hcxselect;


static HtmlLinkArray *AllocateHtmlLinksArray (const size_t num_links);


static HtmlLinkArray *AllocateHtmlLinksArrayFromSet (const hcxselect :: Selector &selector_r, const char *title_selector_s, const char * const data_s, const char * const base_uri_s);

static void ClearHtmlLink (HtmlLink *link_p);
static char *GetInnerText (const htmlcxx :: HTML :: Node *node_p, const char *data_s, ByteBuffer *buffer_p, const bool include_child_text_flag);

static bool InitHtmlLink (HtmlLink *link_p, const char *title_s, const char *uri_s, const char *data_s, const char *base_uri_s);

static json_t *GetHtmlLinkAsJSON (const HtmlLink * const link_p);


HtmlLinkArray *GetLinks (CurlTool *tool_p, const char * const uri_s, const char * const link_selector_s, const char * const title_selector_s)
{
	HtmlLinkArray *links_p = NULL;

	if (uri_s)
		{
			if (!SetUriForCurlTool (tool_p, uri_s))
				{
					return NULL;
				}
		}

	if (RunCurlTool (tool_p) == CURLE_OK)
		{
			const char *data_s = GetCurlToolData (tool_p);

			links_p = GetMatchingLinks (data_s, link_selector_s, title_selector_s, uri_s);
		}

	return links_p;
}



json_t *GetMatchingLinksAsJSON (const char * const data_s, const char * const link_selector_s, const char * const title_selector_s, const char * const base_uri_s)
{
	json_t *res_p = NULL;
	HtmlLinkArray *links_p = GetMatchingLinks (data_s, link_selector_s, title_selector_s, base_uri_s);

	if (links_p)
		{
			res_p = json_array ();

			if (res_p)
				{
					size_t i = links_p -> hla_num_entries;
					HtmlLink *link_p = links_p -> hla_data_p;

					for ( ; i > 0; -- i, ++ link_p)
						{
							json_t *link_json_p = GetHtmlLinkAsJSON (link_p);

							if (link_json_p)
								{
									if (json_array_append_new (res_p, link_json_p) != 0)
										{
											PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to add link for %s to json array", link_p -> hl_uri_s);
											json_decref (link_json_p);
										}
								}
							else
								{
									PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to get json for %s", link_p -> hl_uri_s);
								}
						}

				}		/* if (res_p) */
			else
				{
					PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to create json array for links json");
				}

			FreeHtmlLinkArray (links_p);
		}		/* if (links_p) */
	else
		{
			PrintErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, "Failed to get html links");
		}

	return res_p;
}




static json_t *GetHtmlLinkAsJSON (const HtmlLink * const link_p)
{
	json_t *json_p = NULL;
	char *protocol_s = NULL;
	char *path_s = NULL;

	if (GetDataResourceProtocolAndPath (link_p -> hl_uri_s, &protocol_s, &path_s))
		{
			json_p = GetDataResourceAsJSONByParts (protocol_s, path_s, link_p -> hl_data_s, NULL);

			FreeCopiedString (protocol_s);
			FreeCopiedString (path_s);
		}

	return json_p;
}


HtmlLinkArray *GetMatchingLinks (const char * const data_s, const char * const link_selector_s, const char * const title_selector_s, const char * const base_uri_s)
{
	string source (data_s);
	ParserDom parser;
	tree <htmlcxx :: HTML :: Node> dom = parser.parseTree (source);
	hcxselect :: Selector s (dom);
	bool success_flag = false;
	HtmlLinkArray *links_p = NULL;

	try
		{
			s = s.select (link_selector_s);
			success_flag = true;
		}
	catch (hcxselect::ParseException &ex)
		{
			PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Parse error on '%s' - %s\n", link_selector_s, ex.what ());
		}
	catch (...)
		{
			PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Error parsing '%s'\n", link_selector_s);
		}

	if (success_flag)
		{
			links_p = AllocateHtmlLinksArrayFromSet (s, title_selector_s, data_s, base_uri_s);

		}

	return links_p;
}


static HtmlLinkArray *AllocateHtmlLinksArray (const size_t num_links)
{
	HtmlLink *data_p = (HtmlLink *) AllocMemoryArray (num_links, sizeof (HtmlLink));

	if (data_p)
		{
			HtmlLinkArray *links_p = (HtmlLinkArray *) AllocMemory (sizeof (HtmlLinkArray));

			if (links_p)
				{
					links_p -> hla_data_p = data_p;
					links_p -> hla_num_entries = num_links;

					return links_p;
				}

			FreeMemory (data_p);
		}

	return NULL;
}


static HtmlLinkArray *AllocateHtmlLinksArrayFromSet (const hcxselect :: Selector &selector_r, const char *title_selector_s, const char * const data_s, const char * const base_uri_s)
{
	const size_t num_links = selector_r.size ();
	HtmlLinkArray *links_p = AllocateHtmlLinksArray (num_links);

	if (links_p)
		{
			HtmlLink *link_p = links_p -> hla_data_p;
			ByteBuffer *buffer_p = AllocateByteBuffer (1024);

			if (buffer_p)
				{
					for (hcxselect::Selector::const_iterator it = selector_r.begin (); it != selector_r.end (); ++ it, ++ link_p)
						{
							htmlcxx :: HTML :: Node *node_p = & ((*it) -> data);
							const string &tag_name_r = node_p -> tagName ();

							const map <string, string> &attrs_r = node_p -> attributes ();

							size_t num_attrs = attrs_r.size();

							node_p -> parseAttributes ();

							num_attrs = attrs_r.size();

							#if SELECTOR_DEBUG >= STM_LEVEL_FINEST
							PrintLog (STM_LEVEL_FINEST, __FILE__, __LINE__, "node %s", node_p -> string ().c_data ());
							#endif

							if ((tag_name_r.compare ("a") == 0) || ((tag_name_r.compare ("A") == 0)))
								{
									pair <bool, string> p = node_p -> attribute ("href");

									if (p.first == false)
										{
											p = node_p -> attribute ("HREF");
										}

									if (p.first == true)
										{
											const string &uri_r = p.second;
											const char *title_s = NULL;
											char *inner_text_s = GetInnerText (node_p, data_s, buffer_p, false);

											if (inner_text_s)
												{
													pair <bool, string> title_attr = node_p -> attribute ("title");

													if (title_attr.first == true)
														{
															title_s = title_attr.second.c_str ();
														}

													if (!InitHtmlLink (link_p, title_s, uri_r.c_str (), inner_text_s, base_uri_s))
														{

														}

													FreeCopiedString (inner_text_s);
												}

										}
								}
						}

					FreeByteBuffer (buffer_p);
				}

		}		/* if (links_p) */


	return links_p;
}



static char *GetInnerText (const htmlcxx :: HTML :: Node *node_p, const char *data_s, ByteBuffer *buffer_p, const bool include_child_text_flag)
{
	const string &text_r = node_p -> text ();
	char *inner_text_s = NULL;
	const char *start_p = data_s + (node_p -> offset ()) + text_r.length ();

	if (*start_p)
		{
			const char *end_p = data_s + (node_p -> offset ()) + (node_p -> length ()) - 1;

			while (*end_p && (*end_p != '<') && (start_p < end_p))
				{
					-- end_p;
				}

			if (*end_p == '<')
				{
					bool space_flag = false;
					bool success_flag = true;
					uint32 child_tag_count = 0;

					/* scroll past the < and > */
					-- end_p;

					if (*start_p == '>')
						{
							++ start_p;
						}

					ResetByteBuffer (buffer_p);

					while (isspace (*start_p) != 0)
						{
							++ start_p;
						}

					/*
					 * Make a copy of the inner text whilst reduces any occurrences
					 * of 2 or more consecutive spaces with a single space.
					 */
					while ((start_p <= end_p) && success_flag)
						{
							const char c = *start_p;

							if (isspace (c) != 0)
								{
									if (!space_flag)
										{
											success_flag = AppendToByteBuffer (buffer_p, " ", 1);
											space_flag = true;
										}
								}
							else
								{

									if (c == '<')
										{
											++ child_tag_count;
										}
									else if (c == '>')
										{
											-- child_tag_count;
										}
									else
										{
											if (include_child_text_flag || (child_tag_count == 0))
												{
													if (space_flag)
														{
															space_flag = false;
														}

													success_flag = AppendToByteBuffer (buffer_p, &c, 1);
												}
										}
								}

							++ start_p;
						}

					if (success_flag)
						{
							inner_text_s = EasyCopyToNewString (GetByteBufferData (buffer_p));
						}
				}
		}

	return inner_text_s;
}


void FreeHtmlLinkArray (HtmlLinkArray *links_p)
{
	size_t i = links_p -> hla_num_entries;
	HtmlLink *link_p = links_p -> hla_data_p;

	for ( ; i > 0; -- i, ++ link_p)
		{
			ClearHtmlLink (link_p);
		}

	FreeMemory (links_p -> hla_data_p);
	FreeMemory (links_p);
}


static void ClearHtmlLink (HtmlLink *link_p)
{
	if (link_p -> hl_uri_s)
		{
			FreeCopiedString (link_p -> hl_uri_s);
		}

	if (link_p -> hl_title_s)
		{
			FreeCopiedString (link_p -> hl_title_s);
		}

	if (link_p -> hl_data_s)
		{
			FreeCopiedString (link_p -> hl_data_s);
		}

}


static bool InitHtmlLink (HtmlLink *link_p, const char *title_s, const char *uri_s, const char *data_s, const char *base_uri_s)
{
	char *value_s = NULL;

	if (base_uri_s)
		{
			/* is the uri an absolute uri? */
			const char HTTP_S [] = "http://";
			const char HTTPS_S [] = "https://";

			if ((strncmp (HTTP_S, uri_s, strlen (HTTP_S)) == 0) || (strncmp (HTTPS_S, uri_s, strlen (HTTPS_S)) == 0))
				{
					value_s = EasyCopyToNewString (uri_s);

					if (!value_s)
						{
							PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to copy %s", uri_s);
						}
				}		/* if ((strcmp ("http://", uri_s) == 0) || (strcmp ("http://", uri_s) == 0)) */
			else
				{
					if (*uri_s == '/')
						{
							/* skip past the "://" in the base uri */
							const char delim_s [] = "://";
							const char *slash_p = strstr (base_uri_s, delim_s);

							if (slash_p)
								{
									slash_p += strlen (delim_s);
								}
							else
								{
									slash_p = base_uri_s;
								}

							/* Now find the next slash and append our uri to it */
							slash_p = strchr (slash_p, '/');

							if (slash_p)
								{
									char *root_s = CopyToNewString (base_uri_s, slash_p - base_uri_s, false);

									if (root_s)
										{
											value_s = ConcatenateStrings (root_s, uri_s);
											FreeCopiedString (root_s);
										}		/* if (root_s) */

								}		/* if (slash_p) */

						} 	/* if (*uri_s == '/') */
					else
						{
							const char *last_slash_p = strrchr (base_uri_s, '/');

							if (last_slash_p)
								{
									char *root_path_s = CopyToNewString (base_uri_s, last_slash_p - base_uri_s + 1, false);

									if (root_path_s)
										{
											value_s = ConcatenateStrings (root_path_s, uri_s);
											FreeCopiedString (root_path_s);
										}

								}		/* if (last_slash_p) */

						}

				}

		}		/* if (base_uri_s) */
	else
		{
			value_s = EasyCopyToNewString (uri_s);

			if (!value_s)
				{
					PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to copy %s", uri_s);
				}
		}


	link_p -> hl_uri_s = NULL;
	link_p -> hl_data_s = NULL;
	link_p -> hl_title_s = NULL;

	if (value_s)
		{
			link_p -> hl_uri_s = value_s;

			value_s = EasyCopyToNewString (data_s);

			if (value_s)
				{
					link_p -> hl_data_s = value_s;

					link_p -> hl_title_s = NULL;

					if (title_s)
						{
							value_s = EasyCopyToNewString (title_s);

							if (value_s)
								{
									link_p -> hl_title_s = value_s;
								}
						}

					return true;
				}

			FreeCopiedString (link_p -> hl_uri_s);
			link_p -> hl_uri_s = NULL;
		}

	return false;
}
