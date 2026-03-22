import { NextRequest, NextResponse } from "next/server";
import { createAdminClient } from "@/lib/supabase/admin";
import {
  markPostAsPosting,
  markPostAsPosted,
  markPostAsFailed,
} from "@/lib/social/scheduler";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

/**
 * POST /api/social/publish — Manually publish a scheduled or draft post
 *
 * This route handles the actual publishing to the platform.
 * Platform clients are imported dynamically based on the post's platform.
 */
export async function POST(req: NextRequest) {
  try {
    const body = await req.json();
    const { post_id } = body;

    if (!post_id) {
      return NextResponse.json({ error: "Missing post_id" }, { status: 400 });
    }

    const supabase = createAdminClient();

    // Fetch the post
    const { data: post, error } = await supabase
      .from("social_posts")
      .select("*")
      .eq("id", post_id)
      .single();

    if (error || !post) {
      return NextResponse.json({ error: "Post not found" }, { status: 404 });
    }

    if (post.status === "posted") {
      return NextResponse.json(
        { error: "Post already published" },
        { status: 400 }
      );
    }

    await markPostAsPosting(post_id);

    try {
      let result: { id: string; post_url: string } | null = null;

      switch (post.platform) {
        case "facebook": {
          const fb = await import("@/lib/social/platforms/facebook");
          result = await fb.publishPost({
            message: `${post.caption}\n\n${(post.hashtags || []).map((h: string) => `#${h}`).join(" ")}`,
            imageUrl: post.media_urls?.[0],
          });
          break;
        }

        case "instagram": {
          const ig = await import("@/lib/social/platforms/instagram");
          const caption = `${post.caption}\n\n${(post.hashtags || []).map((h: string) => `#${h}`).join(" ")}`;

          if (post.media_urls?.length > 1) {
            result = await ig.publishCarousel({
              imageUrls: post.media_urls,
              caption,
            });
          } else if (post.media_urls?.[0]) {
            result = await ig.publishPost({
              imageUrl: post.media_urls[0],
              caption,
            });
          }
          break;
        }

        case "twitter": {
          const tw = await import("@/lib/social/platforms/twitter");
          // Upload media if available
          const mediaIds: string[] = [];
          if (post.media_urls?.length > 0) {
            for (const url of post.media_urls.slice(0, 4)) {
              const mediaId = await tw.uploadMedia(url);
              if (mediaId) mediaIds.push(mediaId);
            }
          }
          const tweetText = `${post.caption}\n\n${(post.hashtags || []).slice(0, 3).map((h: string) => `#${h}`).join(" ")}`;
          result = await tw.postTweet({
            text: tweetText.slice(0, 280),
            mediaIds: mediaIds.length > 0 ? mediaIds : undefined,
          });
          break;
        }

        case "bluesky": {
          const bsky = await import("@/lib/social/platforms/bluesky");
          let imageBlob = undefined;
          if (post.media_urls?.[0]) {
            imageBlob = await bsky.uploadImage(post.media_urls[0]);
          }
          const bskyResult = await bsky.createPost({
            text: `${post.caption}\n\n${(post.hashtags || []).slice(0, 5).map((h: string) => `#${h}`).join(" ")}`,
            imageBlob: imageBlob || undefined,
          });
          if (bskyResult) {
            result = { id: bskyResult.uri, post_url: bskyResult.post_url };
          }
          break;
        }

        case "reddit": {
          const reddit = await import("@/lib/social/platforms/reddit");
          const dogUrl = post.dog_id
            ? `https://waitingthelongest.com/dogs/${post.dog_id}`
            : "https://waitingthelongest.com";
          result = await reddit.submitLinkPost({
            subreddit: "AdoptableDogs",
            title: post.caption.split("\n")[0].slice(0, 300),
            url: dogUrl,
          });
          break;
        }

        case "tiktok": {
          const tt = await import("@/lib/social/platforms/tiktok");
          if (post.media_urls?.length > 0) {
            const ttResult = await tt.publishPhotoPost({
              photoUrls: post.media_urls,
              caption: post.caption,
            });
            if (ttResult) {
              result = {
                id: ttResult.publish_id,
                post_url: ttResult.post_url || "",
              };
            }
          }
          break;
        }

        default:
          await markPostAsFailed(post_id, `Platform "${post.platform}" not yet supported`);
          return NextResponse.json(
            { error: `Platform "${post.platform}" not yet supported` },
            { status: 400 }
          );
      }

      if (result) {
        await markPostAsPosted(post_id, result.id, result.post_url);
        return NextResponse.json({
          success: true,
          external_id: result.id,
          post_url: result.post_url,
        });
      } else {
        await markPostAsFailed(post_id, "Platform API returned no result");
        return NextResponse.json(
          { error: "Publishing failed — check platform credentials" },
          { status: 500 }
        );
      }
    } catch (publishErr) {
      const errMsg = publishErr instanceof Error ? publishErr.message : "Unknown error";
      await markPostAsFailed(post_id, errMsg);
      return NextResponse.json(
        { error: `Publishing failed: ${errMsg}` },
        { status: 500 }
      );
    }
  } catch (err) {
    console.error("Social publish error:", err);
    return NextResponse.json(
      { error: "Failed to publish" },
      { status: 500 }
    );
  }
}
